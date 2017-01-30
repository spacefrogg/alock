/*
 * alock - auth_pam.c
 * Copyright (c) 2005 - 2007 Mathias Gumz <akira at fluxbox dot org>
 *               2014 - 2015 Arkadiusz Bokowy
 *
 * This file is a part of an alock.
 *
 * This project is licensed under the terms of the MIT license.
 *
 * This authentication module provides:
 *  -auth pam
 *
 */

#include "alock.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <security/pam_appl.h>


static const char *username = NULL;
static const char *password = NULL;
static char *pam_module = NULL;


static int alock_auth_pam_conv(int num_msg,
        const struct pam_message **msgs,
        struct pam_response **response,
        void *appdata_ptr) {
    (void)appdata_ptr;

    if (num_msg <= 0)
        return PAM_CONV_ERR;

    *response = (struct pam_response *)calloc(num_msg, sizeof(struct pam_response));
    if (*response == NULL)
        return PAM_CONV_ERR;

    int i;
    for (i = 0; i < num_msg; i++) {
        switch (msgs[i]->msg_style) {
        case PAM_PROMPT_ECHO_OFF:
            (*response)[i].resp = strdup(password);
            (*response)[i].resp_retcode = 0;
            break;
        case PAM_ERROR_MSG:
        case PAM_TEXT_INFO:
            fprintf(stderr, "[pam]: %s\n", msgs[i]->msg);
            break;
        default:
            free(*response);
            return PAM_CONV_ERR;
        }
    }

    return PAM_SUCCESS;
}

static int module_init(Display *display) {
    (void)display;

    struct passwd *pwd;

    errno = 0;
    if (!(pwd = getpwuid(getuid()))) {
        perror("[pam]: password entry for uid not found");
        return -1;
    }

    username = pwd->pw_name;
    return 0;
}

static int module_authenticate(const char *pass) {

    if (!username)
        return -1;

    pam_handle_t *pam_handle = NULL;
    struct pam_conv conv = {
        &alock_auth_pam_conv, NULL
    };
    int retval;

    password = pass;
    retval = pam_start(pam_module, username, &conv, &pam_handle);

    if (retval == PAM_SUCCESS)
        retval = pam_set_item(pam_handle, PAM_TTY, ttyname(0));
    if (retval == PAM_SUCCESS)
        retval = pam_authenticate(pam_handle, 0);

    pam_end(pam_handle, retval);
    return !(retval == PAM_SUCCESS);
}

static void module_cmd_list(void) {

    printf("list of available PAM module options:\n"
           "  module=NAME\tModule name to use under /etc/pam.d/ to authenticate\n");
}

static void module_loadargs(const char *args) {

    if (!args || strstr(args, "pam:") != args)
      goto end;

    char *arguments = strdup(&args[4]);
    char *arg;
    char *tmp;

    for (tmp = arguments; tmp; ) {
        arg = strsep(&tmp, ",");
        if (strcmp(arg, "list") == 0) {
            module_cmd_list();
            exit(EXIT_SUCCESS);
        }
        if (strstr(arg, "module=") == arg) {
            pam_module = strdup(&arg[7]);
        }
    }

 end:
    if (!pam_module)
      pam_module = strdup("system-auth");
}

static void module_free(void) {
    free(pam_module);
    pam_module = NULL;
}

struct aModuleAuth alock_auth_pam = {
    { "pam",
        module_loadargs,
        module_dummy_loadxrdb,
        module_init,
        module_free,
    },
    module_authenticate,
};
