program bpb(output);
var
    i: integer;
    fi: file of integer;
    t: text;
begin
    { get }

    get;
      {^ error:parameter-count-mismatch }
    get(i);
       {^ error:type-mismatch }
    get(fi:3);
         {^ error:disallowed-parameter-form }
    get(fi, 1);
           {^ error:parameter-count-mismatch }

    { reset }

    reset;
        {^ error:parameter-count-mismatch }
    reset(i);
         {^ error:type-mismatch }
    reset(fi:3);
           {^ error:disallowed-parameter-form }
    reset(fi, 1);
             {^ error:parameter-count-mismatch }

    { rewrite }

    rewrite;
          {^ error:parameter-count-mismatch }
    rewrite(i);
           {^ error:type-mismatch }
    rewrite(fi:3);
             {^ error:disallowed-parameter-form }
    rewrite(fi, 1);
               {^ error:parameter-count-mismatch }

    { put }

    put;
      {^ error:parameter-count-mismatch }
    put(i);
       {^ error:type-mismatch }
    put(fi:3);
         {^ error:disallowed-parameter-form }
    put(fi, 1);
           {^ error:parameter-count-mismatch }

    { write (???) }

    write;
        {^ error:parameter-count-mismatch }

    { write (typed) }

    write(fi);
           {^ error:parameter-count-mismatch }

    write(fi.x, 1);
           {^ error:non-record-type } { testing that the error is only emitted once }

    write(fi:3, 1);
           {^ error:disallowed-parameter-form }

    write(fi, 1:3);
              {^ error:disallowed-parameter-form }

    write(fi, 1.1);
             {^ error:type-mismatch }

    { write (text) }

    write(t);
          {^ error:parameter-count-mismatch }

    write(t.x, 1);
          {^ error:non-record-type } { testing that the error is only emitted once }

    write(t:3, 1);
          {^ error:disallowed-parameter-form }

    write(t, t);
            {^ error:type-mismatch }

    write(t, 1:1.1);
              {^ error:type-mismatch }

    write(t, 1:1:1.1);
                {^ error:disallowed-parameter-form }

    write(t, 1.1:1:1.1);
                  {^ error:type-mismatch }

    write(nil);
         {^ error:type-mismatch }

    write(1:1.1);
           {^ error:type-mismatch }

    write(1:1:1.1);
             {^ error:disallowed-parameter-form }

    write(1.1:1:1.1);
               {^ error:type-mismatch }

    { writeln }

    writeln(t.x);
            {^ error:non-record-type } { testing that the error is only emitted once }

    writeln(t:3);
            {^ error:disallowed-parameter-form }

    writeln(t, t);
              {^ error:type-mismatch }

    writeln(t, 1:1.1);
                {^ error:type-mismatch }

    writeln(t, 1:1:1.1);
                  {^ error:disallowed-parameter-form }

    writeln(t, 1.1:1:1.1);
                    {^ error:type-mismatch }

    writeln(fi);
           {^ error:type-mismatch }

    writeln(1:1.1);
             {^ error:type-mismatch }

    writeln(1:1:1.1);
               {^ error:disallowed-parameter-form }

    writeln(1.1:1:1.1);
                 {^ error:type-mismatch }
end.
