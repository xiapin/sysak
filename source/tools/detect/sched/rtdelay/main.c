#include "rtdelay_api.h"
#include <argp.h>
#include <time.h>
#include <string.h>

const char *argp_program_version = "rtdelay 0.1";
const char argp_program_doc[] =
    "Summarize off-CPU time(in us) by stack trace.\n"
    "\n"
    "USAGE: rtdelay [--help] [-p PID]  [-d DURATION] [-s ServerPID] [-u]\n"
    "EXAMPLES:\n"
    "    rtdelay                     # trace RT time until Ctrl-C\n"
    "    rtdelay -p 185              # only trace threads for PID 185\n"
    "    rtdelay -p 185 -s 827732    # only trace threads for PID 185 and server PID is 827732\n"
    "    rtdelay -u                  # show result in user-friendly interface \n"
    "    rtdelay -d 10               # trace for 10 seconds only\n";

static const struct argp_option opts[] = {
    {"pid", 'p', "PID", 0, "Trace this PID only"},
    {"server", 's', "SeverPID", 0, "Trace Server PID"},
    {"duration", 'd', "DURATION", 0, "Total duration of trace in seconds"},
    {"user", 'u', NULL, 0, "Show result in user-friendly interface"},
    {NULL, 'h', NULL, OPTION_HIDDEN, "Show the full help"},
    {},
};

static struct env
{
    pid_t pid;
    time_t duration;
    pid_t server_pid;
    int json_out;
} env = {
    .pid = -1,
    .duration = 99999999,
    .server_pid = -1,
    .json_out = 1,
};

static error_t parse_arg(int key, char *arg, struct argp_state *state)
{
    time_t duration;

    switch (key)
    {
    case ARGP_KEY_ARG:
        argp_usage(state);
        break;
    case 'u':
        env.json_out = 0;
        break;
    case 'h':
        argp_state_help(state, stderr, ARGP_HELP_STD_HELP);
        break;
    case 'p':
        errno = 0;
        env.pid = strtol(arg, NULL, 10);
        if (errno)
        {
            fprintf(stderr, "invalid PID: %s\n", arg);
            argp_usage(state);
        }
        break;
    case 's':
        errno = 0;
        env.server_pid = strtol(arg, NULL, 10);
        if (errno)
        {
            fprintf(stderr, "invalid PID: %s\n", arg);
            argp_usage(state);
        }
        break;
    case 'd':
        errno = 0;
        duration = strtol(arg, NULL, 10);
        if (errno || duration <= 0)
        {
            fprintf(stderr, "invalid DURATION: %s\n", arg);
            argp_usage(state);
        }
        env.duration = duration;
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

int main(int argc, char **argv)
{

    int err;
    static const struct argp argp = {
        .options = opts,
        .parser = parse_arg,
        .doc = argp_program_doc,
    };

    /* Parse command line arguments */
    err = argp_parse(&argp, argc, argv, 0, NULL, NULL);
    if (err)
        return err;

    err = rtdelay(env.pid, env.server_pid, env.duration, env.json_out);

    return err < 0 ? -err : 0;
}
