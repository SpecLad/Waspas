program badparams;
procedure p(i: integer; procedure r);
    begin end;
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
end.
