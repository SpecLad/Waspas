{
SPDX-FileCopyrightText: (C) Roman Donchenko <rdonchen@outlook.com>

SPDX-License-Identifier: MPL-2.0
}

program badlabeldecl;
label 123, abc;
          {^ error:unexpected-token }
begin
end.
