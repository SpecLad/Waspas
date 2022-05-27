program dup;
const
    a = 1;
   {^ note }
    a = 1;
   {^ error:duplicate-identifier }
begin
end.
