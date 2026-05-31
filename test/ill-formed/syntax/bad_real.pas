{
SPDX-FileCopyrightText: (C) Roman Donchenko <rdonchen@outlook.com>

SPDX-License-Identifier: MPL-2.0
}

program badrealbig;
const x = 2e308;
         {^ error:invalid-real }
begin
end.
