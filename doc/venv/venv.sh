#!/bin/bash

ALIYUN_MIRROR="https://mirrors.aliyun.com/pypi/simple/"
export SERVER_HOME=$(pwd)

VIRTUALENV_HOME="${SERVER_HOME}/virtualenv"

touch_virtualenv() {
    pip3.9 uninstall virtualenv -y
    pip3.6 install virtualenv -i "${ALIYUN_MIRROR}"
    if [ -d ${VIRTUALENV_HOME} ]; then
        echo "virtualenv exists, skip"
        echo "INFO: activate virtualenv..."
        source ${VIRTUALENV_HOME}/bin/activate || exit 1
    else
        virtualenv  ${VIRTUALENV_HOME}
        if [ "$?" = 0 ]; then
            echo "INFO: create virtualenv success"
        else
            echo "ERROR: create virtualenv failed"
            exit 1
        fi
        echo "INFO: activate virtualenv..."
        source ${VIRTUALENV_HOME}/bin/activate || exit 1
        check_requirements
    fi
}


check_requirements() {
    echo "INFO: begin install requirements..."
    if ! [ -d ${SERVER_HOME}/logs/ ]; then
        mkdir -p ${SERVER_HOME}/logs/ || exit 1
    fi

    local requirements_log="${SERVER_HOME}/logs/requirements.log"
    local requirements="requirements.txt"
    touch "$requirements_log" || exit
    pip3.6 install --upgrade pip
    pip3.6 install -r ${requirements} -i "${ALIYUN_MIRROR}" |tee -a "${requirements_log}" || exit 1
	local pip_res=$?
    if [ $pip_res -ne 0 ]; then
        echo "ERROR: requirements not satisfied and auto install failed, please check ${requirements_log}"
        exit 1
    fi
}

pyinstaller_sar() {
    echo "INFO: begin pyinstaller sar..."
    pyinstaller -F  sample.py --add-data './config.yaml:./' -y
    deactivate
    rm -rf ${VIRTUALENV_HOME}
}

deploy() {
    touch_virtualenv
    pyinstaller_sar
}

deploy
