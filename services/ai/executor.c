#include "executor.h"

static int plan_structure_valid(const struct nova_ai_plan *plan) {
    size_t i, j;
    if (plan == 0 || plan->version != NOVA_AI_PLAN_VERSION ||
        plan->step_count == 0U ||
        plan->step_count > NOVA_AI_PLAN_MAX_STEPS) return 0;
    for (i = 0U; i < plan->step_count; ++i) {
        const struct nova_ai_plan_step *step = &plan->steps[i];
        if (step->operation_id == 0U || step->capability_id == 0U ||
            step->schema_version == 0U || step->effects == 0U ||
            (step->effects & ~NOVA_AI_EFFECT_MASK) != 0U ||
            ((step->effects & NOVA_AI_EFFECT_DESTRUCTIVE) != 0U &&
             (step->effects & NOVA_AI_EFFECT_WRITE) == 0U) ||
            step->payload_size > NOVA_AI_PLAN_MAX_PAYLOAD) return 0;
        for (j = 0U; j < i; ++j)
            if (plan->steps[j].operation_id == step->operation_id)
                return 0;
    }
    return 1;
}

static void block_remaining(struct nova_ai_execution_state *state,
                            size_t first) {
    size_t i;
    for (i = first; i < state->step_count; ++i)
        if (state->steps[i] == NOVA_AI_STEP_PENDING)
            state->steps[i] = NOVA_AI_STEP_BLOCKED;
}

int nova_ai_plan_execute(const struct nova_ai_plan *plan,
                         nova_ai_authorize_fn authorize,
                         nova_ai_execute_fn execute,
                         nova_ai_cancel_fn should_cancel,
                         struct nova_ai_execution_state *state,
                         void *context) {
    size_t i;
    if (state == 0) return 0;
    state->step_count = 0U;
    state->completed_steps = 0U;
    state->failed_step = UINT16_MAX;
    state->cancelled = 0;
    for (i = 0U; i < NOVA_AI_PLAN_MAX_STEPS; ++i)
        state->steps[i] = NOVA_AI_STEP_PENDING;
    if (!plan_structure_valid(plan) || authorize == 0 || execute == 0 ||
        should_cancel == 0) return 0;

    state->step_count = plan->step_count;
    for (i = 0U; i < plan->step_count; ++i) {
        const struct nova_ai_plan_step *step = &plan->steps[i];
        if (should_cancel(context)) {
            state->cancelled = 1;
            while (i < state->step_count)
                state->steps[i++] = NOVA_AI_STEP_CANCELLED;
            return 0;
        }
        if (!authorize(step->capability_id, step->schema_version,
                       step->effects, step->payload_size, context)) {
            state->steps[i] = NOVA_AI_STEP_DENIED;
            state->failed_step = (uint16_t)i;
            block_remaining(state, i + 1U);
            return 0;
        }
        state->steps[i] = NOVA_AI_STEP_RUNNING;
        if (!execute(step, context)) {
            state->steps[i] = NOVA_AI_STEP_FAILED;
            state->failed_step = (uint16_t)i;
            block_remaining(state, i + 1U);
            return 0;
        }
        state->steps[i] = NOVA_AI_STEP_SUCCEEDED;
        ++state->completed_steps;
    }
    return 1;
}
