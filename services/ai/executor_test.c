#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "executor.h"

struct test_context {
    unsigned int authorize_calls;
    unsigned int execute_calls;
    unsigned int cancel_checks;
    uint32_t deny_capability;
    uint32_t fail_operation;
    int cancel_after_first;
};

static int authorize(uint32_t capability, uint16_t version, uint16_t effects,
                     size_t payload_size, void *opaque) {
    struct test_context *ctx = (struct test_context *)opaque;
    ++ctx->authorize_calls;
    return capability != ctx->deny_capability && version == 1U &&
           effects != 0U && payload_size <= NOVA_AI_PLAN_MAX_PAYLOAD;
}

static int execute(const struct nova_ai_plan_step *step, void *opaque) {
    struct test_context *ctx = (struct test_context *)opaque;
    ++ctx->execute_calls;
    return step->operation_id != ctx->fail_operation;
}

static int cancelled(void *opaque) {
    struct test_context *ctx = (struct test_context *)opaque;
    ++ctx->cancel_checks;
    return ctx->cancel_after_first && ctx->execute_calls != 0U;
}

static struct nova_ai_plan make_plan(void) {
    struct nova_ai_plan plan = {0};
    plan.version = NOVA_AI_PLAN_VERSION;
    plan.step_count = 3U;
    plan.steps[0] = (struct nova_ai_plan_step){1U, 10U, 1U,
                                               NOVA_AI_EFFECT_READ, 16U};
    plan.steps[1] = (struct nova_ai_plan_step){2U, 11U, 1U,
                                               NOVA_AI_EFFECT_WRITE, 24U};
    plan.steps[2] = (struct nova_ai_plan_step){3U, 12U, 1U,
                                               NOVA_AI_EFFECT_READ, 8U};
    return plan;
}

int main(void) {
    struct nova_ai_plan plan = make_plan();
    struct nova_ai_execution_state state;
    struct test_context ctx = {0};

    assert(nova_ai_plan_execute(&plan, authorize, execute, cancelled,
                                &state, &ctx));
    assert(ctx.authorize_calls == 3U && ctx.execute_calls == 3U);
    assert(state.completed_steps == 3U && !state.cancelled);
    assert(state.steps[0] == NOVA_AI_STEP_SUCCEEDED);
    assert(state.steps[2] == NOVA_AI_STEP_SUCCEEDED);

    ctx = (struct test_context){0};
    ctx.deny_capability = 11U;
    assert(!nova_ai_plan_execute(&plan, authorize, execute, cancelled,
                                 &state, &ctx));
    assert(ctx.execute_calls == 1U);
    assert(state.steps[0] == NOVA_AI_STEP_SUCCEEDED);
    assert(state.steps[1] == NOVA_AI_STEP_DENIED);
    assert(state.steps[2] == NOVA_AI_STEP_BLOCKED);

    ctx = (struct test_context){0};
    ctx.cancel_after_first = 1;
    assert(!nova_ai_plan_execute(&plan, authorize, execute, cancelled,
                                 &state, &ctx));
    assert(ctx.execute_calls == 1U && state.cancelled);
    assert(state.steps[0] == NOVA_AI_STEP_SUCCEEDED);
    assert(state.steps[1] == NOVA_AI_STEP_CANCELLED);
    assert(state.steps[2] == NOVA_AI_STEP_CANCELLED);

    ctx = (struct test_context){0};
    ctx.fail_operation = 2U;
    assert(!nova_ai_plan_execute(&plan, authorize, execute, cancelled,
                                 &state, &ctx));
    assert(state.steps[0] == NOVA_AI_STEP_SUCCEEDED);
    assert(state.steps[1] == NOVA_AI_STEP_FAILED);
    assert(state.steps[2] == NOVA_AI_STEP_BLOCKED);

    puts("Nova OS AI plan execution tests passed.");
    return 0;
}
