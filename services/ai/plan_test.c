#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "plan.h"

struct policy_state {
    int allow_write;
    int allow_network;
    unsigned int calls;
};

static int authorize(uint32_t capability_id, uint16_t version,
                     uint16_t effects, size_t payload_size, void *context) {
    struct policy_state *policy = (struct policy_state *)context;
    ++policy->calls;
    if (capability_id == 0U || version != 1U ||
        payload_size > NOVA_AI_PLAN_MAX_PAYLOAD)
        return 0;
    if ((effects & NOVA_AI_EFFECT_WRITE) != 0U && !policy->allow_write)
        return 0;
    if ((effects & NOVA_AI_EFFECT_NETWORK) != 0U && !policy->allow_network)
        return 0;
    return 1;
}

int main(void) {
    struct nova_ai_plan plan = {0};
    struct policy_state policy = {0, 0, 0};

    plan.version = NOVA_AI_PLAN_VERSION;
    plan.step_count = 2U;
    plan.steps[0].operation_id = 1U;
    plan.steps[0].capability_id = 10U;
    plan.steps[0].schema_version = 1U;
    plan.steps[0].effects = NOVA_AI_EFFECT_READ;
    plan.steps[0].payload_size = 32U;
    plan.steps[1].operation_id = 2U;
    plan.steps[1].capability_id = 11U;
    plan.steps[1].schema_version = 1U;
    plan.steps[1].effects = NOVA_AI_EFFECT_WRITE;
    plan.steps[1].payload_size = 16U;

    assert(!nova_ai_plan_validate(&plan, authorize, &policy));
    assert(policy.calls == 2U);
    policy.allow_write = 1;
    policy.calls = 0U;
    assert(nova_ai_plan_validate(&plan, authorize, &policy));

    plan.steps[1].operation_id = plan.steps[0].operation_id;
    assert(!nova_ai_plan_validate(&plan, authorize, &policy));
    plan.steps[1].operation_id = 2U;
    plan.steps[1].effects = NOVA_AI_EFFECT_DESTRUCTIVE;
    assert(!nova_ai_plan_validate(&plan, authorize, &policy));
    plan.steps[1].effects = NOVA_AI_EFFECT_WRITE;
    plan.steps[1].payload_size = NOVA_AI_PLAN_MAX_PAYLOAD + 1U;
    assert(!nova_ai_plan_validate(&plan, authorize, &policy));
    plan.steps[1].payload_size = 16U;
    plan.step_count = NOVA_AI_PLAN_MAX_STEPS + 1U;
    assert(!nova_ai_plan_validate(&plan, authorize, &policy));
    plan.step_count = 2U;
    plan.version = 99U;
    assert(!nova_ai_plan_validate(&plan, authorize, &policy));

    puts("Nova OS AI plan validation tests passed.");
    return 0;
}
