program badstmt;
label 1;
begin
    1: ;
   {^ note }
    1: ;
   {^ error:ambiguous-label }
    2: ;
   {^ error:undefined-label }
end.
