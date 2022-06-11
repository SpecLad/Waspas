program test(
    a,
   {^note}
    b,
   {^ error:missing-program-parameter-variable }
    a);
   {^ error:duplicate-program-parameter }
var
    a: text;
begin
end.
