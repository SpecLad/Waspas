program badconst;
const
    a = b;
       {^ error:undefined-identifier }
    b = 123;
    c = '1';
    d = 'many';
    e = +c;
       {^ error:type-mismatch }
    f = -d;
       {^ error:type-mismatch }
begin
end.
