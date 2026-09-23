/ replay.q
/ Usage from OrderBook/: q q/replay.q -logFile log/tplog_YYYY.MM.DD

/ 1. Parse command line arguments
defaultLogFile:"log/tplog_",string .z.D;
logFileArg:defaultLogFile;
i:0;
while[i<count .z.x;
    if["-logFile"~.z.x i; if[i+1<count .z.x; logFileArg:.z.x i+1]];
    i+:1
    ];
logFile:hsym `$logFileArg;

/ 2. Define the exact physical schema layout matching the payload
\l common/schema.q
depth:depthSchema;

/ 3. Use a flexible insert that forces list assignment 
.u.upd:{[tbl;payload] 
    tbl insert payload;
 };

/ 4. Direct Replay Execution (No complex trap function to obscure errors)
-11!logFile;

/ 5. Print status
-1 "Successfully replayed records.";
-1 "depth total: ", string count depth;
show 3#depth;
