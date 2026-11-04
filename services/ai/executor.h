#ifndef NOVA_AI_EXECUTOR_H
#define NOVA_AI_EXECUTOR_H

#include "plan.h"

enum nova_ai_step_state {
    NOVA_AI_STEP_PENDING = 0,
    NOVA_AI_STEP_RUNNING,
    NOVA_AI_STEP_SUCCEEDED,
    NOVA_AI_STEP_DENIED,
    NOVA_AI_STEP_FAILED,
    NOVA_AI_STEP_CANCELLED,
    NOVA_AI_STEP_BLOCKED
};

struct nova_ai_execution_state {
    enum nova_ai_step_state steps[NOVA_AI_PLAN_MAX_STEPS];
    uint16_t step_count;
    uint16_t completed_steps;
    uint16_t failed_step;
    int cancelled;
};

typedef int (*nova_ai_execute_fn)(const struct nova_ai_plan_step *step,
                                  void *context);
typedef int (*nova_ai_cancel_fn)(void *context);

/*
 * Executes a validated plan sequentially. Authorization is checked just
 * before each step so revocation can block later work. A failed or denied
 * step blocks all following steps; cancellation is checked at each boundary.
 */
int nova_ai_plan_execute(const struct nova_ai_plan *plan,
                         nova_ai_authorize_fn authorize,
                         nova_ai_execute_fn execute,
                         nova_ai_cancel_fn should_cancel,
                         struct nova_ai_execution_state *state,
                         void *context);

#endif
