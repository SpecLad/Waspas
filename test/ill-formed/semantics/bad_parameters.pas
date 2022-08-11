program badparams;
var
    goodArray: array [1..10] of integer;
    arrayWithDifferentBounds: array [-10..-1] of integer;
procedure p(i: integer; procedure r);
    begin end;
procedure q(a, b: array[m..n: integer] of integer);
    begin
        q(a, goodArray);
         {^ error:disallowed-parameter-form }
    end;
procedure r; begin end;

begin
    p;
    {^ error:parameter-count-mismatch }
    p(1);
      {^ error:parameter-count-mismatch }
    p(1, r, 1);
           {^ error:parameter-count-mismatch }

    p(2.3, r);
     {^ error:type-mismatch }

    p(1:10, r);
      {^ error:disallowed-parameter-form }

    q(goodArray, arrayWithDifferentBounds);
     {^ note }  {^ error:type-mismatch }
end.
