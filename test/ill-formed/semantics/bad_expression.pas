program badexpr;
const
    c = 0;
type
    pint = ^integer;
function f(a: array[m..n: integer] of integer): pint;
    begin
        f := nil;

        m := 0;
       {^ error:wrong-identifier-kind }

        f^ := 0;
        {^ error:invalid-component-access }
    end;
begin
    undefined := 0;
   {^ error:undefined-identifier }
    c := 1;
   {^ error:wrong-identifier-kind }
    f := nil;
   {^ error:wrong-identifier-kind }
end.
