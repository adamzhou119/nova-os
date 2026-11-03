#ifndef NOVA_AI_PLAN_H
#define NOVA_AI_PLAN_H

#include <stddef.h>
#include <stdint.h>

#define NOVA_AI_PLAN_VERSION 1U
#define NOVA_AI_PLAN_MAX_STEPS 8U
#define NOVA_AI_PLAN_MAX_PAYLOAD 256U

#define NOVA_AI_EFFECT_READ        (1U << 0)
#define NOVA_AI_EFFECT_WRITE       (1U << 1)
#define NOVA_AI_EFFECT_NETWORK     (1U << 2)
#define NOVA_AI_EFFECT_DESTRUCTIVE (1U << 3)
#define NOVA_AI_EFFECT_SENSITIVE   (1U << 4)
#define NOVA_AI_EFFECT_MASK        ((1U << 5) - 1U)

struct nova_ai_plan_step {
    uint32_t operation_id;
    uint32_t capability_id;
    uint16_t schema_version;
    uint16_t effects;
    uint16_t payload_size;
};

struct nova_ai_plan {
    uint16_t version;
    uint16_t step_count;
    struct nova_ai_plan_step steps[NOVA_AI_PLAN_MAX_STEPS];
};

/*
 * Trusted broker callback. It must enforce caller identity, current grants,
 * data scope, and fresh user consent for sensitive/destructive effects.
 * Model output never supplies or overrides the authorization decision.
 */
typedef int (*nova_ai_authorize_fn)(uint32_t capability_id,
                                    uint16_t schema_version,
                                    uint16_t effects,
                                    size_t payload_size,
                                    void *context);

/* Validates structure and asks the trusted policy broker to authorize every step. */
int nova_ai_plan_validate(const struct nova_ai_plan *plan,
                          nova_ai_authorize_fn authorize,
                          void *context);

#endif
