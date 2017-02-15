#!/usr/bin/env bash

# FIXME(bbannier): module docstring

set -e
set -o pipefail

function random_port {
  # Generate a random port number.
  echo $((RANDOM + 2000))
}

function setup_env {
  # shellcheck source=/dev/null
  source "${MESOS_SOURCE_DIR}"/support/colors.sh
  # shellcheck source=/dev/null
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

function cleanup {
  rm -f framework_id
}

function start_master {
  MESOS_WORK_DIR=$(mktemp -d -t mesos-master-XXXXXX)
  atexit rm -rf "${MESOS_WORK_DIR}"

  MASTER_PORT=$(random_port)

  ACLS=${1:-\{\"permissive\": true\}}

  ${MASTER} \
    --ip=127.0.0.1 \
    --port="$MASTER_PORT" \
    --acls="${ACLS}" \
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

  RESOURCES=${1:-cpus:1;mem:96;disk:50}

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
  echo "${GREEN}Running framework${NORMAL}"
  ROLES=${1:-\[\"roleA\", \"roleB\"\]}
  DEFAULT_TASKS='
      {
        "tasks": [
          {
            "role": "roleA",
            "task": {
              "command": { "value": "sleep 1" },
              "name": "task1",
              "task_id": { "value": "task1" },
              "resources": [
                {
                  "name": "cpus",
                  "scalar": {
                    "value": 0.5
                  },
                  "type": "SCALAR"
                },
                {
                  "name": "mem",
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
                  "scalar": {
                    "value": 0.5
                  },
                  "type": "SCALAR"
                },
                {
                  "name": "mem",
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

  MESOS_TASKS=${MESOS_TASKS:-$DEFAULT_TASKS}

  ${MULTIROLE_FRAMEWORK} \
    --master=127.0.0.1:"$MASTER_PORT" \
    --roles="$ROLES" \
    --max_unsuccessful_offer_cycles=3 \
    --tasks="${MESOS_TASKS}"
}

setup_env

function test_1 {
  echo "${BOLD}"
  echo "********************************************************************************************"
  echo "* A framework can be in two roles and start tasks on resources allocated for either role.  *"
  echo "********************************************************************************************"
  echo "${NORMAL}"
  start_master
  start_agent
  run_framework
}

function test_quota {
  echo "${BOLD}"
  echo "********************************************************************************************"
  echo "* Frameworks in multiple roles can use quota.                                              *"
  echo "********************************************************************************************"
  echo "${NORMAL}"
  start_master
  start_agent

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

  curl --silent -d"${QUOTA}" http://127.0.0.1:"$MASTER_PORT"/quota

  echo "${BOLD}"
  echo The framework will not get any resources to run tasks with 'roleB'.
  echo "${NORMAL}"

  ! run_framework

  echo "${BOLD}"
  echo If we make more resources available, the framework will also be offered resources for 'roleB'.
  echo "${NORMAL}"
  start_agent

  run_framework
}

function test_reserved_resources {
  echo "${BOLD}"
  echo "********************************************************************************************"
  echo "* Reserved resources.                                                                      *"
  echo "********************************************************************************************"
  echo "${NORMAL}"
  start_master

  echo "${BOLD}"
  RESOURCES="cpus(roleA):0.5;cpus(roleB):0.5;mem(roleA):48;mem(roleB):48;disk(roleA):25;disk(roleB):25"
  echo Starting agent with reserved resources: $RESOURCES.
  echo We expect a framework in both roles to be able to launch tasks on resources from either role.
  echo "${NORMAL}"
  start_agent "${RESOURCES}"
  run_framework
}

function test_fair_share {
  echo "${BOLD}"
  echo "********************************************************************************************"
  echo "* Fair share.                                                                              *"
  echo "********************************************************************************************"
  echo "Starting a cluster with two frameworks: one framework is in two roles"
  echo "['roleA', 'roleB'] with one task in each, the other is only ['roleA']"
  echo "with two tasks."
  echo "We also start three agents which fit exactly one workload of the"
  echo "frameworks. We expect one framework to be able to launch both of its tasks immediately,"
  echo "while the other one will have to wait."
  echo "${NORMAL}"
  start_master

  MESOS_TASKS='
  {
    "tasks": [
    {
      "role": "roleA",
      "task": {
        "command": { "value": "sleep 1" },
        "name": "task1",
        "task_id": { "value": "task1" },
        "resources": [
          {
            "name": "cpus",
            "scalar": { "value": 0.5 },
            "type": "SCALAR"
          },
          {
            "name": "mem",
            "scalar": { "value": 48 },
            "type": "SCALAR"
          }
        ],
        "slave_id": { "value": "" }
        }
      }, {
      "role": "roleB",
      "task": {
        "command": { "value": "sleep 1" },
        "name": "task2",
        "task_id": { "value": "task2" },
        "resources": [
          {
            "name": "cpus",
            "scalar": { "value": 0.5 },
            "type": "SCALAR"
          },
          {
            "name": "mem",
            "scalar": { "value": 48 },
            "type": "SCALAR"
          }
        ],
        "slave_id": { "value": "" }
      }}
    ]
  }'

  echo "${BOLD}"
  echo Starting a framework in two roles which will consume the bulk on the resources.
  echo "${NORMAL}"
  run_framework &

  start_agent "cpus:0.5;mem:48;disk:25"
  start_agent "cpus:0.5;mem:48;disk:25"
  start_agent "cpus:0.5;mem:48;disk:25"

  echo "${BOLD}"
  echo "Starting a framework in just one role which will be offered not enough"
  echo "resources initially since the earlier one will be below fair share in"
  echo "that role ('taskX_one_role' will finish last)."
  echo "${NORMAL}"

  # TODO(bbannier): Make this more testable. We expect this second framework to
  # finish last.
  cleanup
  (MESOS_TASKS=$(echo ${MESOS_TASKS} | sed 's/roleB/roleA/g' | sed 's/task1/task1_one_role/g' | sed 's/task2/task2_one_role/g') run_framework '["roleA"]')
}

function test_framework_authz {
  echo "${BOLD}"
  echo "********************************************************************************************"
  echo "* Framework authorization.                                                                 *"
  echo "********************************************************************************************"
  echo "${NORMAL}"

  ACLS='
  {
    "permissive": false,
    "register_frameworks": [
      {
        "principals": { "values": ["'${DEFAULT_PRINCIPAL}'"] },
        "roles": { "values": ["roleA", "roleB"] }
      },
      {
        "principals": { "values": ["OTHER_PRINCIPAL"] },
        "roles": { "values" : ["roleB"] }
      }
    ],
    "run_tasks": [
      {
        "principals" : { "values": ["'${DEFAULT_PRINCIPAL}'"] },
        "users": { "type": "ANY" }
      }
    ]
  }
  '

  CREDENTIALS='
  {
    "credentials": [
    {
      "principal": "'$DEFAULT_PRINCIPAL'",
      "secret": "'$DEFAULT_SECRET'"
    },
    {
      "principal": "OTHER_PRINCIPAL",
      "secret": "secret"
    }
    ]
  }'

  echo "${CREDENTIALS}" > credentials.json
  MESOS_CREDENTIALS=file://$(realpath credentials.json)
  export MESOS_CREDENTIALS

  echo "${BOLD}"
  echo "Using the following ACLs:"
  echo "${ACLS}" | python -m json.tool
  echo "${NORMAL}"

  start_master "${ACLS}"
  start_agent

  echo "${BOLD}"
  echo "Attempting to register a framework in role 'roleB' with a"
  echo "principal authorized for the role succeeds."
  echo "${NORMAL}"
  (DEFAULT_PRINCIPAL='OTHER_PRINCIPAL' DEFAULT_SECRET='secret' MESOS_TASKS='{"tasks": []}' run_framework '["roleB"]')

  echo "${BOLD}"
  echo "Attempting to register a framework in roles ['roleA', 'roleB'] with a principal authorized only for 'roleB' fails."
  echo "${NORMAL}"
  cleanup
  ! (DEFAULT_PRINCIPAL='OTHER_PRINCIPAL' DEFAULT_SECRET='secret' MESOS_TASKS='{"tasks": []}' run_framework)

  echo "${BOLD}"
  echo "Attempting to register a framework in roles ['roleA', 'roleB'] with a"
  echo "principal authorized for both roles succeeds. The framework can"
  echo "run tasks."
  cleanup
  echo "${NORMAL}"
  run_framework
}

function test_failover {
  echo "${BOLD}"
  echo "********************************************************************************************"
  echo "* A framework changing its roles can learn about its previous tasks.                       *"
  echo "********************************************************************************************"
  echo "${NORMAL}"
  start_master
  start_agent

  TASKS='
  {
    "tasks": [
    {
      "role": "roleA",
      "await": false,
      "task": {
        "command": { "value": "sleep 2" },
        "name": "task1",
        "task_id": { "value": "task1" },
        "resources": [
          {
            "name": "cpus",
            "scalar": { "value": 0.5 },
            "type": "SCALAR"
          },
          {
            "name": "mem",
            "scalar": { "value": 48 },
            "type": "SCALAR"
          }
        ],
        "slave_id": { "value": "" }
        }
      }, {
      "role": "roleB",
      "await": false,
      "task": {
        "command": { "value": "sleep 2" },
        "name": "task2",
        "task_id": { "value": "task2" },
        "resources": [
          {
            "name": "cpus",
            "scalar": { "value": 0.5 },
            "type": "SCALAR"
          },
          {
            "name": "mem",
            "scalar": { "value": 48 },
            "type": "SCALAR"
          }
        ],
        "slave_id": { "value": "" }
      }}
    ]
  }'

  (MESOS_TASKS="${TASKS}" run_framework '["roleA", "roleB"]')

  echo "${BOLD}"
  echo "Restarting framework dropping 'roleA'. We can reconcile tasks started with dropped roles."
  echo "${NORMAL}"
  (MESOS_TASKS='{"tasks": []}' run_framework '["roleB"]')
}

test_1
cleanup

# test_failover
# cleanup

test_reserved_resources
cleanup

test_fair_share
cleanup

test_quota
cleanup

test_framework_authz
cleanup
