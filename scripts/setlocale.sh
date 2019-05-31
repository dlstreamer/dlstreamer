# Below is a workaround for OpenVINO Inference Engine to work correctly on some systems
# with unexpected system locale settings (e.g., on systems with RU locale).
# If Inference Engine is used with such unexpected locales, we assume it may produce wrong results
# due to incorrect work with floating point
export LC_NUMERIC="C"
echo [setlocale.sh] C locale is set
