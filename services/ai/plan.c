#include "plan.h"

static int duplicate_operation_id(const struct nova_ai_plan *plan,
                                  size_t index) {
    size_t i;
    for (i = 0U; i < index; ++i)
        if (plan->steps[i].operation_id == plan->steps[index].operation_id)
            return 1;
    return 0;
}

int nova_ai_plan_validate(const struct nova_ai_plan *plan,
                          nova_ai_authorize_fn authorize,
                          void *context) {
    size_t i;
    if (plan == 0 || authorize == 0 ||
        plan->version != NOVA_AI_PLAN_VERSION ||
        plan->step_count == 0U ||
        plan->step_count > NOVA_AI_PLAN_MAX_STEPS)
        return 0;

    for (i = 0U; i < plan->step_count; ++i) {
        const struct nova_ai_plan_step *step = &plan->steps[i];
        if (step->operation_id == 0U ||
            step->capability_id == 0U ||
            step->schema_version == 0U ||
            step->effects == 0U ||
            (step->effects & ~NOVA_AI_EFFECT_MASK) != 0U ||
            ((step->effects & NOVA_AI_EFFECT_DESTRUCTIVE) != 0U &&
             (step->effects & NOVA_AI_EFFECT_WRITE) == 0U) ||
            step->payload_size > NOVA_AI_PLAN_MAX_PAYLOAD ||
            duplicate_operation_id(plan, i))
            return 0;

        if (!authorize(step->capability_id, step->schema_version,
                       step->effects, step->payload_size, context))
            return 0;
    }
    return 1;
}
