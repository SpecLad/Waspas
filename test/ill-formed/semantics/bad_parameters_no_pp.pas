{ This test is for builtin procedure calls that are invalid due to a missing
  program parameter. }

program bpnpp;
var
    i: integer;
begin
    read(i);
        {^ error:undefined-identifier }

    readln;
         {^ error:undefined-identifier }
    readln(i);
          {^ error:undefined-identifier }

    write(1);
         {^ error:undefined-identifier }

    writeln;
          {^ error:undefined-identifier }
    writeln(1);
           {^ error:undefined-identifier }
end.
