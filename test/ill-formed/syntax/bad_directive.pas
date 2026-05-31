{
SPDX-FileCopyrightText: (C) Roman Donchenko <rdonchen@outlook.com>

SPDX-License-Identifier: MPL-2.0
}

program baddirective;
procedure foo; bar;
              {^ error:invalid-directive }
begin
end.
