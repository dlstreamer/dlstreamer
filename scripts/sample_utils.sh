get_option()
{
  echo $1 | cut --delimiter='=' --fields=2
}

get_source_element()
{
  local RESULT=
  if [[ ${1} == "/dev/video"* ]]; then
    RESULT="v4l2src device=${1}"
  elif [[ ${1} == "rtsp://"* ]]; then
    RESULT="urisourcebin uri=${1}"
  elif [[ ! -z ${1} ]]; then
    if [[ -f ${1} ]]; then
      RESULT="filesrc location=\"$(realpath ${1})\""
    else
      echo -e "\e[31mERROR: Can't find ${1}\e[0m" 1>&2
    fi
  fi
  if [[ -z ${RESULT} ]]; then
    echo -e "\e[31mERROR: Can't get a video stream from ${1}\e[0m" 1>&2
  fi
  echo ${RESULT}
}

supported_video_sources()
{
    echo "  local file: /path/on/local/file/system"
    echo "  web-camera: /dev/video*"
    echo "  network:    rtsp://*"
}
