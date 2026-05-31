{
SPDX-FileCopyrightText: (C) Roman Donchenko <rdonchen@outlook.com>

SPDX-License-Identifier: MPL-2.0
}

program badint;
const x = 2147483648;
         {^ error:invalid-integer }
begin
end.
