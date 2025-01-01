1. Run qoiMPI\x64\Debug\qoiMPI.exe by cmd or powershell
2. input format mpiexec -n [number of process] [dir.exe] [mode(encode/decode)] [input dir] [output dir]
3. example run command: mpiexec -n 4 "C:\Users\yangy\source\repos\qoiMPI\x64\Debug\qoiMPI.exe" decode C:\\ZheYangBackup\\QOIoutput C:\\ZheYangBackup\\encodedOutput