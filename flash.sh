suggested_esptool_command=$(make all | tee /dev/stderr | tail -n 1)

echo "-----------------------------------------------"
echo "Invoking powershell.exe..."

powershell.exe -command "${suggested_esptool_command}"
