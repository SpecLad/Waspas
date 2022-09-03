{ This test is for builtin procedure calls that are invalid due to a missing
  program parameter. }

program bpnpp;
begin
    write(1);
         {^ error:undefined-identifier }

    writeln;
          {^ error:undefined-identifier }
    writeln(1);
           {^ error:undefined-identifier }
end.
