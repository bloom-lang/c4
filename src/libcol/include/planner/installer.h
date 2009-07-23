#ifndef INSTALLER_H
#define INSTALLER_H

#include "planner/planner.h"

void install_plan(ProgramPlan *plan, apr_pool_t *pool, ColInstance *col);

#endif  /* INSTALLER_H */
