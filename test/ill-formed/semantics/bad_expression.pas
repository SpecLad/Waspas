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

        i := m^;
             {^ error:invalid-component-access }

        aa := aa[1.1];
                {^ error:type-mismatch }
    end;
begin
    undefined := 0;
   {^ error:undefined-identifier }
    c := 1;
   {^ error:wrong-identifier-kind }
    f := nil;
   {^ error:wrong-identifier-kind }

    i := c^;
         {^ error:invalid-component-access }

    i := i.a;
         {^ error:non-record-type }
    r := r.a;
          {^ error:undefined-identifier }

    i := i^;
         {^ error:type-mismatch }

    i := i[0];
          {^ error:non-array-type }

    a := a[1.1];
          {^ error:type-mismatch }
end.
