#pragma once

int InitMecab();
wchar_t *MecabParseString(const wchar_t *string, int len, wchar_t **error = 0);
