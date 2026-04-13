{ This test is for builtin procedure calls that are invalid due to a missing
  program parameter. }

program bpnpp;
var
    b: boolean;
    i: integer;
begin
    b := eof;
           {^ error:undefined-identifier }

    b := eoln;
            {^ error:undefined-identifier }

    page;
       {^ error:undefined-identifier }

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
