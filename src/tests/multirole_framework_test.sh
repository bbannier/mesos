#!/usr/bin/env bash

# FIXME(bbannier): module docstring

set -e
set -o pipefail

function random_port {
  # Generate a random port number.
  echo $(($RANDOM + 2000))
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
    --acls='{"permissive": true}' \
    --work_dir="${MESOS_WORK_DIR}" &> "${MESOS_WORK_DIR}.log" &
  MASTER_PID=${!}

  atexit rm -rf "${MESOS_WORK_DIR}.log"

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

  AGENT_PORT=$(random_port)

  RESOURCES=$1

  ${AGENT} \
    --work_dir="${MESOS_WORK_DIR}" \
    --runtime_dir="${MESOS_RUNTIME_DIR}" \
    --master=127.0.0.1:"$MASTER_PORT" \
    --port="$AGENT_PORT" \
    --resources="${RESOURCES}" &> "${MESOS_WORK_DIR}.log" &
  AGENT_PID=${!}

  atexit rm -rf "${MESOS_WORK_DIR}.log"

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

function run_framework {
  ${MULTIROLE_FRAMEWORK} \
    --master=127.0.0.1:"$MASTER_PORT" \
    --roles='["roleA", "roleB"]' \
    --max_unsuccessful_offer_cycles=3 \
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
                    "value": 0.5
                  },
                  "type": "SCALAR"
                },
                {
                  "name": "mem",
                  "role": "*",
                  "scalar": {
                    "value": 48
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
                    "value": 0.5
                  },
                  "type": "SCALAR"
                },
                {
                  "name": "mem",
                  "role": "*",
                  "scalar": {
                    "value": 48
                  },
                  "type": "SCALAR"
                }
              ],
              "slave_id": { "value": "" }
            }
          }
        ]
      }'
}

setup_env

function test_1 {
  echo "${BOLD}"
  echo "********************************************************************************************"
  echo "* A framework can be in two roles and start tasks on resources allocated for either role.  *"
  echo "********************************************************************************************"
  echo "${NORMAL}"
  start_master
  start_agent "cpus:1;mem:96;disk:50"
  run_framework
}

function test_2 {
  echo "${BOLD}"
  echo "********************************************************************************************"
  echo "* Frameworks in multiple roles can use quota.                                              *"
  echo "********************************************************************************************"
  echo "${NORMAL}"
  start_master
  start_agent "cpus:1;mem:96;disk:50"

  echo "${BOLD}"
  echo "Quota'ing all of the agent's resources for 'roleA'."
  echo "${NORMAL}"
  QUOTA='
  {
    "role": "roleA",
    "force": true,
    "guarantee": [
    {
      "name": "cpus",
      "type": "SCALAR",
      "scalar": { "value": 1}
    },
    {
      "name": "mem",
      "type": "SCALAR",
      "scalar": { "value": 96}
    },
    {
      "name": "disk",
      "type": "SCALAR",
      "scalar": { "value": 50}
    }
    ]
  }'

  curl --verbose -d"${QUOTA}" http://127.0.0.1:"$MASTER_PORT"/quota

  echo "${BOLD}"
  echo The framework will not get any resources to run tasks with 'roleB'.
  echo "${NORMAL}"
  [ ! $(run_framework) ]

  echo "${BOLD}"
  echo If we make more resources available, the framework will also be offered resources for 'roleB'.
  echo "${NORMAL}"
  start_agent "cpus:1;mem:96;disk:50"

  run_framework
}

function test_reserved_resources {
  echo "${BOLD}"
  echo "********************************************************************************************"
  echo "* Reserved resources                                                                       *"
  echo "********************************************************************************************"
  echo "${NORMAL}"
  start_master

  echo "${BOLD}"
  RESOURCES="cpus(roleA):0.5;cpus(roleB):0.5;mem(roleA):48;mem(roleB):48;disk(roleA):25;disk(roleB):25"
  echo Starting agent with reserved resources: $RESOURCES.
  start_agent "${RESOURCES}"
  echo "${NORMAL}"
  run_framework
}

# test_1
# test_2
test_reserved_resources
