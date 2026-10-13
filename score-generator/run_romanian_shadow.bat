@echo off
rem Builds the "Joc de Umbre" score for the romanian_shadow style profile.
rem   run_romanian_shadow.bat                      write docs\example_score_ro_joc_de_umbre.json
rem   run_romanian_shadow.bat --list               print what each voice plays
rem   run_romanian_shadow.bat --dump-plan my.json  write the plan as an editable file
rem   run_romanian_shadow.bat --plan my.json       build from your edited plan
rem (any argument of romanian_shadow.py works; see its --help)
python "%~dp0romanian_shadow.py" %*
echo.
pause
