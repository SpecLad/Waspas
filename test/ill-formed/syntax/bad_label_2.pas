{
SPDX-FileCopyrightText: (C) Roman Donchenko <rdonchen@outlook.com>

SPDX-License-Identifier: MPL-2.0
}

program badlabel2;
label 99999999999999999999999999999999999999999;
     {^ error:invalid-label }
begin
end.
