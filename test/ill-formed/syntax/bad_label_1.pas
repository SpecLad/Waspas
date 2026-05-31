{
SPDX-FileCopyrightText: (C) Roman Donchenko <rdonchen@outlook.com>

SPDX-License-Identifier: MPL-2.0
}

program badlabel1;
label 10000;
     {^ error:invalid-label }
begin
end.
