#!/usr/bin/env bash

# FIXME(bbannier): module docstring

set -e
set -o pipefail

function random_port {
  # Generate a random port number in the range [2000, 65000].
  # We use `awk` here as alternatives like `shuf` or `jot` are not as
  # universally available (only on Linux or BSDs, respectively).
  awk 'BEGIN{srand(); print int(rand()*(63000-2000))+2000 }'
}

function setup_env {
  source "${MESOS_SOURCE_DIR}"/support/colors.sh
  source "${MESOS_SOURCE_DIR}"/support/atexit.sh

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
}

function start_master {
  MESOS_WORK_DIR=$(mktemp -d -t mesos-master-XXXXXX)
  atexit rm -rf "${MESOS_WORK_DIR}"

  MASTER_PORT=$(random_port)

  ${MASTER} \
    --ip=127.0.0.1 \
    --port="$MASTER_PORT" \
    --work_dir="${MESOS_WORK_DIR}" &> "${MESOS_WORK_DIR}.log" &
  MASTER_PID=${!}

  atexit rm "${MESOS_WORK_DIR}.log"

  echo "${GREEN}Launched master at ${MASTER_PID}${NORMAL}"

  sleep 2

  # Check the master is still running after 2 seconds.
  kill -0 ${MASTER_PID} >/dev/null 2>&1
  STATUS=${?}
  if [[ ${STATUS} -ne 0 ]]; then
    echo "${RED}Master crashed; failing test${NORMAL}"
    exit 2
  fi

  atexit kill ${MASTER_PID}
}

function start_agent {
  # Disable support for systemd as this test does not run as root.
  # This flag must be set as an environment variable because the flag
  # does not exist on non-Linux builds.
  export MESOS_SYSTEMD_ENABLE_SUPPORT=false

  MESOS_WORK_DIR=$(mktemp -d -t mesos-agent-XXXXXX)
  atexit rm -rf "${MESOS_WORK_DIR}"

  MESOS_RUNTIME_DIR=$(mktemp -d -t mesos-agent-runtime-XXXXXX)
  atexit rm -rf "${MESOS_RUNTIME_DIR}"

  ${AGENT} \
    --work_dir="${MESOS_WORK_DIR}" \
    --runtime_dir="${MESOS_RUNTIME_DIR}" \
    --master=127.0.0.1:"$MASTER_PORT" \
    --resources="cpus:1;mem:96;disk:50" &> "${MESOS_WORK_DIR}.log" &
  AGENT_PID=${!}

  atexit rm "${MESOS_WORK_DIR}.log"

  echo "${GREEN}Launched agent at ${AGENT_PID}${NORMAL}"

  sleep 2

  # Check the agent is still running after 2 seconds.
  kill -0 ${AGENT_PID} >/dev/null 2>&1
  STATUS=${?}
  if [[ ${STATUS} -ne 0 ]]; then
    echo "${RED}Slave crashed; failing test${NORMAL}"
    exit 2
  fi

  atexit kill ${AGENT_PID}
}

setup_env
start_master
start_agent

# The main event!
${MULTIROLE_FRAMEWORK} \
    --master=127.0.0.1:"$MASTER_PORT" \
    --roles='["roleA", "roleB"]' \
    --tasks='
      {
        "tasks": [
          {
            "role": "roleA",
            "task": {
              "command": { "value": "sleep 2" },
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
