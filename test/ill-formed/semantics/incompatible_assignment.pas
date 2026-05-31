{
SPDX-FileCopyrightText: (C) Roman Donchenko <rdonchen@outlook.com>

SPDX-License-Identifier: MPL-2.0
}

program incompassign;
type
    color = (red, green, blue);
var
    f: file of integer;
    af: array [1..10] of file of integer;

    vint: integer;
    vreal: real;

    vintrange: 0..10;
    vcolorrange: red..green;

    vintset: set of integer;
    vintrangeset: set of 0..10;
    vcolorset: set of color;
    vcolorrangeset: set of red..green;

    vstring: packed array [1..10] of char;
    vstringwronglen: packed array [1..11] of char;
    vstringwronglowerbound: packed array [0..10] of char;
    vstringunpacked: array [1..10] of char;
    vstringnonchar: packed array [1..10] of integer;

begin
    f := f;
        {^ error:type-mismatch}
    af := af;
         {^ error:type-mismatch}

    vint := vreal;
           {^ error:type-mismatch}

    vint := red;
           {^ error:type-mismatch}
    vint := vcolorrange;
           {^ error:type-mismatch}
    vintrange := red;
                {^ error:type-mismatch}
    vintrange := vcolorrange;
                {^ error:type-mismatch}

    vintset := vcolorset;
              {^ error:type-mismatch}
    vintset := vcolorrangeset;
              {^ error:type-mismatch}
    vintrangeset := vcolorset;
                   {^ error:type-mismatch}
    vintrangeset := vcolorrangeset;
                   {^ error:type-mismatch}

    vstringwronglen := vstring;
                      {^ error:type-mismatch}
    vstringwronglowerbound := vstring;
                             {^ error:type-mismatch}
    vstringunpacked := vstring;
                      {^ error:type-mismatch}
    vstringnonchar := vstring;
                     {^ error:type-mismatch}
end.
