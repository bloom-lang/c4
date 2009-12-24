#ifndef INSTALLER_H
#define INSTALLER_H

#include "planner/planner.h"

void install_plan(ProgramPlan *plan, apr_pool_t *pool, C4Instance *c4);

#endif  /* INSTALLER_H */
