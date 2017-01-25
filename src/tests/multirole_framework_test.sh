#!/usr/bin/env bash


# FIXME(bbannier): module docstring

set -e
set -o pipefail

source ${MESOS_SOURCE_DIR}/support/colors.sh
source ${MESOS_SOURCE_DIR}/support/atexit.sh

MASTER_PID=
AGENT_PID=
MESOS_WORK_DIR=`mktemp -d -t mesos-XXXXXX`
MESOS_RUNTIME_DIR=`mktemp -d -t mesos-XXXXXX`


function cleanup() {
  # Make sure we kill the master on exit.
  if [[ ! -z ${MASTER_PID} ]]; then
    kill ${MASTER_PID}
  fi

  # Make sure we kill the agent on exit.
  if [[ ! -z ${AGENT_PID} ]]; then
    kill ${AGENT_PID}
  fi

  if [[ -d "${MESOS_WORK_DIR}" ]]; then
    rm -rf "${MESOS_WORK_DIR}"
  fi

  if [[ -d "${MESOS_RUNTIME_DIR}" ]]; then
    rm -rf "${MESOS_RUNTIME_DIR}"
  fi
}

atexit cleanup

export LD_LIBRARY_PATH=${MESOS_BUILD_DIR}/src/.libs
MASTER=${MESOS_SBIN_DIR}/mesos-master
AGENT=${MESOS_SBIN_DIR}/mesos-agent
MULTIROLE_FRAMEWORK=${MESOS_HELPER_DIR}/multirole-framework

# The mesos binaries expect MESOS_ prefixed environment variables
# to correspond to flags, so we unset these here.
unset MESOS_BUILD_DIR
unset MESOS_SOURCE_DIR
unset MESOS_HELPER_DIR
unset MESOS_VERBOSE

# Launch master.
${MASTER} \
    --ip=127.0.0.1 \
    --port=5432 \
    --work_dir="${MESOS_WORK_DIR}" &
MASTER_PID=${!}
echo "${GREEN}Launched master at ${MASTER_PID}${NORMAL}"
# sleep 2 # FIXME(bbannier): reneable?

# Check the master is still running after 2 seconds.
kill -0 ${MASTER_PID} >/dev/null 2>&1
STATUS=${?}
if [[ ${STATUS} -ne 0 ]]; then
  echo "${RED}Master crashed; failing test${NORMAL}"
  exit 2
fi

# Disable support for systemd as this test does not run as root.
# This flag must be set as an environment variable because the flag
# does not exist on non-Linux builds.
export MESOS_SYSTEMD_ENABLE_SUPPORT=false

# Launch agent.
${AGENT} \
    --work_dir="${MESOS_WORK_DIR}" \
    --runtime_dir="${MESOS_RUNTIME_DIR}" \
    --master=127.0.0.1:5432 \
    --resources="cpus:1;mem:96;disk:50" &
AGENT_PID=${!}
echo "${GREEN}Launched agent at ${AGENT_PID}${NORMAL}"
# sleep 2 # FIXME(bbannier): reenable?

# Check the agent is still running after 2 seconds.
kill -0 ${AGENT_PID} >/dev/null 2>&1
STATUS=${?}
if [[ ${STATUS} -ne 0 ]]; then
  echo "${RED}Slave crashed; failing test${NORMAL}"
  exit 2
fi

# The main event!
${MULTIROLE_FRAMEWORK} \
    --master=127.0.0.1:5432 \
    --roles='["roleA", "roleB"]' \
    --tasks='
      {
        "tasks": [
          {
            "role": "roleA",
            "task": {
              "command": { "value": "sleep 3" },
              "name": "task1",
              "task_id": { "value": "task1" },
              "resources": [
                {
                  "name": "cpus",
                  "role": "*",
                  "scalar": {
                    "value": 0.1
                  },
                  "type": "SCALAR"
                },
                {
                  "name": "mem",
                  "role": "*",
                  "scalar": {
                    "value": 32
                  },
                  "type": "SCALAR"
                }
              ],
              "slave_id": { "value": "" }
            }
          },
          {
            "role": "roleB",
            "task": {
              "command": { "value": "sleep 1" },
              "name": "task2",
              "task_id": { "value": "task2" },
              "resources": [
                {
                  "name": "cpus",
                  "role": "*",
                  "scalar": {
                    "value": 0.1
                  },
                  "type": "SCALAR"
                },
                {
                  "name": "mem",
                  "role": "*",
                  "scalar": {
                    "value": 32
                  },
                  "type": "SCALAR"
                }
              ],
              "slave_id": { "value": "" }
            }
          }
        ]
      }'
STATUS=${?}

# # Make sure the disk full framework "failed".
# if [[ ! ${STATUS} -eq 1 ]]; then
#   echo "${RED} Disk full framework returned ${STATUS} not 1${NORMAL}"
#   exit 1
# fi

# And make sure the agent is still running!
kill -0 ${AGENT_PID} >/dev/null 2>&1
STATUS=${?}
if [[ ${STATUS} -ne 0 ]]; then
  echo "${RED}Slave crashed; failing test${NORMAL}"
  exit 2
fi

exit 0
