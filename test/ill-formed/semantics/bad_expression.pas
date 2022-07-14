program badexpr;
const
    c = 0;
type
    pint = ^integer;
    rec = record end;
var
    i: integer;
    r: rec;
    a: array [1..10] of integer;
function f(aa: array [m..n: integer] of integer): pint;
    begin
        f := nil;

        m := 0;
       {^ error:wrong-identifier-kind }

        f^ := 0;
        {^ error:invalid-component-access }

        i := aa[1.1];
               {TODO: error:type-mismatch }
    end;
begin
    undefined := 0;
   {^ error:undefined-identifier }
    c := 1;
   {^ error:wrong-identifier-kind }
    f := nil;
   {^ error:wrong-identifier-kind }

    i := i.a;
        {TODO: error:non-record-type }
    i := r.a;
          {TODO: error:undefined-identifier }

    i := i^;
         {TODO: error:type-mismatch }

    i := i[0];
          {TODO: error:non-array-type }

    i := a[1.1];
          {TODO: error:type-mismatch }
end.
