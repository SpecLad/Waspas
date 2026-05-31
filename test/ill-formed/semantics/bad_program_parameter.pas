{
SPDX-FileCopyrightText: (C) Roman Donchenko <rdonchen@outlook.com>

SPDX-License-Identifier: MPL-2.0
}

program test(
    a,
   {^note}
    b,
   {^ error:missing-program-parameter-variable }
    a);
   {^ error:duplicate-program-parameter }
var
    a: text;
begin
end.
