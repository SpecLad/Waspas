{
SPDX-FileCopyrightText: (C) Roman Donchenko <rdonchen@outlook.com>

SPDX-License-Identifier: MPL-2.0
}

program test;
label 1000, 2000, 1000;
     {^ note }   {^ error:duplicate-label }
begin
    1000:;
    2000:;
end.
