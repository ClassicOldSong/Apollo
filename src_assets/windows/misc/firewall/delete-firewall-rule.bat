@echo off

set RULE_NAME=Apollo

rem Delete the rule
netsh advfirewall firewall delete rule name=%RULE_NAME%
