GET_MODEL_PATH() {
    model_name=$1
    for models_dir in ${MODELS_PATH//:/ }; do
        paths=$(find $models_dir -name "*$model_name.xml" -exec grep "version=\"4\"" -l {} \;)
        if [ ! -z "$paths" ];
        then
            echo $(echo "$paths" | head -n 1)
            exit 0
        fi
    done

    echo -e "\e[31mModel $model_name file was not found. Please set MODELS_PATH\e[0m" 1>&2
    exit 1
}

PROC_PATH() {
    echo ${GST_SAMPLES_DIR}/model_proc/$1.json
}
