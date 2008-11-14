/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 *  (C) 2008 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "hydra.h"
#include "hydra_mem.h"
#include "csi.h"

HYD_CSI_Handle csi_handle;

HYD_Status HYD_LCHU_Create_host_list(void)
{
    FILE *fp;
    char line[2 * MAX_HOSTNAME_LEN], *hostfile, *hostname, *procs;
    struct HYD_CSI_Proc_params *proc_params;
    int i, j, num_procs;
    HYD_Status status = HYD_SUCCESS;

    HYDU_FUNC_ENTER();

    /* FIXME: We need a better approach than this -- we make two
     * passes for the total host list, one to find the number of
     * hosts, and another to read the actual hosts. */
    proc_params = csi_handle.proc_params;
    while (proc_params) {
        if (proc_params->host_file != NULL) {
            if (!strcmp(proc_params->host_file, "HYDRA_USE_LOCALHOST")) {
                proc_params->total_num_procs++;
            }
            else {
                fp = fopen(proc_params->host_file, "r");
                if (fp == NULL) {
                    HYDU_Error_printf("unable to open host file %s\n", proc_params->host_file);
                    status = HYD_INTERNAL_ERROR;
                    goto fn_fail;
                }

                proc_params->total_num_procs = 0;
                while (!feof(fp)) {
                    if ((fscanf(fp, "%s", line) < 0) && errno) {
                        HYDU_Error_printf("unable to read input line (errno: %d)\n", errno);
                        status = HYD_INTERNAL_ERROR;
                        goto fn_fail;
                    }
                    if (feof(fp))
                        break;

                    hostname = strtok(line, ":");
                    procs = strtok(NULL, ":");
                    if (procs)
                        num_procs = atoi(procs);
                    else
                        num_procs = 1;

                    proc_params->total_num_procs += num_procs;
                }

                fclose(fp);
            }
        }
        proc_params = proc_params->next;
    }

    proc_params = csi_handle.proc_params;
    while (proc_params) {
        if (proc_params->host_file != NULL) {

            HYDU_MALLOC(proc_params->total_proc_list, char **,
                        proc_params->total_num_procs * sizeof(char *), status);
            HYDU_MALLOC(proc_params->total_core_list, int *,
                        proc_params->total_num_procs * sizeof(int), status);

            if (!strcmp(proc_params->host_file, "HYDRA_USE_LOCALHOST")) {
                proc_params->total_proc_list[0] = MPIU_Strdup("localhost");
                proc_params->total_core_list[0] = -1;
            }
            else {
                fp = fopen(proc_params->host_file, "r");
                if (fp == NULL) {
                    HYDU_Error_printf("unable to open host file %s\n", proc_params->host_file);
                    status = HYD_INTERNAL_ERROR;
                    goto fn_fail;
                }

                i = 0;
                while (!feof(fp)) {
                    if ((fscanf(fp, "%s", line) < 0) && errno) {
                        HYDU_Error_printf("unable to read input line (errno: %d)\n", errno);
                        status = HYD_INTERNAL_ERROR;
                        goto fn_fail;
                    }
                    if (feof(fp))
                        break;

                    hostname = strtok(line, ":");
                    procs = strtok(NULL, ":");

                    if (procs)
                        num_procs = atoi(procs);
                    else
                        num_procs = 1;

                    for (j = 0; j < num_procs; j++) {
                        proc_params->total_proc_list[i] = MPIU_Strdup(hostname);
                        proc_params->total_core_list[i] = -1;
                        i++;
                    }
                }

                fclose(fp);
            }
        }
        proc_params = proc_params->next;
    }

  fn_exit:
    HYDU_FUNC_EXIT();
    return status;

  fn_fail:
    goto fn_exit;
}


HYD_Status HYD_LCHU_Free_host_list(void)
{
    struct HYD_CSI_Proc_params *proc_params;
    int i;
    HYD_Status status = HYD_SUCCESS;

    HYDU_FUNC_ENTER();

    proc_params = csi_handle.proc_params;
    while (proc_params) {
        for (i = 0; i < proc_params->total_num_procs; i++)
            HYDU_FREE(proc_params->total_proc_list[i]);
        HYDU_FREE(proc_params->total_proc_list);
        HYDU_FREE(proc_params->total_core_list);
        HYDU_FREE(proc_params->host_file);
        proc_params = proc_params->next;
    }

    HYDU_FUNC_EXIT();
    return status;
}


HYD_Status HYD_LCHU_Create_env_list(void)
{
    struct HYD_CSI_Proc_params *proc_params;
    HYDU_Env_t *env;
    HYD_Status status = HYD_SUCCESS;

    HYDU_FUNC_ENTER();

    if (csi_handle.prop == HYDU_ENV_PROP_ALL) {
        csi_handle.prop_env = HYDU_Env_listdup(csi_handle.global_env);
        for (env = csi_handle.user_env; env; env = env->next) {
            status = HYDU_Env_add_to_list(&csi_handle.prop_env, *env);
            if (status != HYD_SUCCESS) {
                HYDU_Error_printf("unable to add env to list\n");
                goto fn_fail;
            }
        }
    }

    proc_params = csi_handle.proc_params;
    while (proc_params) {
        if (proc_params->prop == HYDU_ENV_PROP_ALL) {
            proc_params->prop_env = HYDU_Env_listdup(csi_handle.global_env);
            for (env = proc_params->user_env; env; env = env->next) {
                status = HYDU_Env_add_to_list(&proc_params->prop_env, *env);
                if (status != HYD_SUCCESS) {
                    HYDU_Error_printf("unable to add env to list\n");
                    goto fn_fail;
                }
            }
        }
        proc_params = proc_params->next;
    }

  fn_exit:
    HYDU_FUNC_EXIT();
    return status;

  fn_fail:
    goto fn_exit;
}


HYD_Status HYD_LCHU_Free_env_list(void)
{
    struct HYD_CSI_Proc_params *proc_params;
    HYD_Status status = HYD_SUCCESS;

    HYDU_FUNC_ENTER();

    HYDU_Env_free_list(csi_handle.global_env);
    HYDU_Env_free_list(csi_handle.system_env);
    HYDU_Env_free_list(csi_handle.user_env);
    HYDU_Env_free_list(csi_handle.prop_env);

    proc_params = csi_handle.proc_params;
    while (proc_params) {
        HYDU_Env_free_list(proc_params->user_env);
        HYDU_Env_free_list(proc_params->prop_env);
        proc_params = proc_params->next;
    }

    HYDU_FUNC_EXIT();
    return status;
}


HYD_Status HYD_LCHU_Free_io(void)
{
    struct HYD_CSI_Proc_params *proc_params;
    HYD_Status status = HYD_SUCCESS;

    HYDU_FUNC_ENTER();

    proc_params = csi_handle.proc_params;
    while (proc_params) {
        HYDU_FREE(proc_params->out);
        HYDU_FREE(proc_params->err);
        proc_params = proc_params->next;
    }

    HYDU_FUNC_EXIT();
    return status;
}


HYD_Status HYD_LCHU_Free_exits(void)
{
    struct HYD_CSI_Proc_params *proc_params;
    HYD_Status status = HYD_SUCCESS;

    HYDU_FUNC_ENTER();

    proc_params = csi_handle.proc_params;
    while (proc_params) {
        HYDU_FREE(proc_params->exit_status);
        proc_params = proc_params->next;
    }

    HYDU_FUNC_EXIT();
    return status;
}


HYD_Status HYD_LCHU_Free_exec(void)
{
    struct HYD_CSI_Proc_params *proc_params;
    int i;
    HYD_Status status = HYD_SUCCESS;

    HYDU_FUNC_ENTER();

    proc_params = csi_handle.proc_params;
    while (proc_params) {
        for (i = 0; proc_params->exec[i]; i++)
            HYDU_FREE(proc_params->exec[i]);
        proc_params = proc_params->next;
    }

    HYDU_FUNC_EXIT();
    return status;
}


HYD_Status HYD_LCHU_Free_proc_params(void)
{
    struct HYD_CSI_Proc_params *proc_params, *run;
    HYD_Status status = HYD_SUCCESS;

    HYDU_FUNC_ENTER();

    proc_params = csi_handle.proc_params;
    while (proc_params) {
        run = proc_params->next;
        HYDU_FREE(proc_params);
        proc_params = run;
    }

    HYDU_FUNC_EXIT();
    return status;
}
