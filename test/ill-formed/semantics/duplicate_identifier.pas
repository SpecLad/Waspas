{
SPDX-FileCopyrightText: (C) Roman Donchenko <rdonchen@outlook.com>

SPDX-License-Identifier: MPL-2.0
}

program dup;
const
    a = 1;
   {^ note }
    a = 1;
   {^ error:duplicate-identifier }
type
    b = integer;
   {^ note }
    b = real;
   {^ error:duplicate-identifier }
    enumType = (c,        c);
               {^ note } {^ error:duplicate-identifier }

    recordSelector = 1..2;
    recordType = record
        a: integer;
       {^ note }
        a: real;
       {^ error:duplicate-identifier }

        b: integer;
       {^ note }

        c: real;
       {^ note }

        case d: recordSelector of
            {^ note }
            1: (case b: recordSelector of 1: (); 2: ());
                    {^ error:duplicate-identifier }
            2: (
                c: text;
               {^ error:duplicate-identifier }
                d: boolean;
               {^ error:duplicate-identifier }
            );
    end;
var
    d: integer;
   {^ note }
    d: real;
   {^ error:duplicate-identifier }
procedure e; begin end;
         {^ note }
procedure e; begin end;
         {^ error:duplicate-identifier }
function f: integer; begin f := 0 end;
        {^ note }
function f: integer; begin f := 0 end;
        {^ error:duplicate-identifier }

procedure proc1(
    a,       a: integer;
   {^ note }{^ error:duplicate-identifier }
    procedure b; procedure b;
             {^ note }    {^ error:duplicate-identifier }
    function c: integer; function c: integer;
            {^ note }            {^ error:duplicate-identifier }
    d,       d:
   {^ note }{^ error:duplicate-identifier }
        array [e..      e: integer] of integer
              {^ note }{^ error:duplicate-identifier }
);
    begin end;

procedure proc2(a: integer);
               {^ note }
    var a: integer;
       {^ error:duplicate-identifier }
    begin end;
begin
end.
