program baddirective;
procedure foo; bar;
              {^ error:invalid-directive }
begin
end.
