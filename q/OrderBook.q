/ ==============================================================================
/ FUNCTION: GetOrderBook
/ PURPOSE: Collapses any pre-filtered historical depth slice into a true order book
/ INPUT:   table - a pre-filtered table (e.g., filtered by sym, date, or time)
/ ==============================================================================
GetOrderBook:{[depthTable]
    / 1. Collapse updates using 'by'. 'last' guarantees the final absolute state.
    collapsed: select last date, last time, last sym, last received_at, last size by side, price from depthTable;
        
    / 2. Filter out cancelled levels (size = 0)
    live: select from collapsed where size > 0;
    
    / 3. Corrected: Flatten the keyed structure back to a regular, ordered table
    :0!live
 };


/ ==============================================================================
/ UTILITY FUNCTIONS: View top levels
/ ==============================================================================
GetBids:{[bookTable; n] select[n; >price] from bookTable where side=`buy};
GetAsks:{[bookTable; n] select[n; <price] from bookTable where side=`sell};


/ ==============================================================================
/ REAL-TIME LIVE ENGINE
/ ==============================================================================

/ 1. Initialize an empty in-memory live order book cache
liveBook: ([sym:`symbol$(); side:`symbol$(); price:`float$()] time:`timestamp$(); received_at:`timestamp$(); size:`float$());

/ 2. Fast engine function to update the live book state with incoming rows
UpdLiveBook:{[newData]
    / 1. Upsert incoming data directly into the keyed cache
    liveBook :: liveBook upsert select last time, last received_at, last size by sym, side, price from newData;
    
    / 2. Clean up explicitly cancelled price levels (size = 0)
    liveBook :: delete from liveBook where size = 0;
    
    / 3. Drop stale levels older than 2 seconds to reduce crossed quotes
    / We look at the latest timestamp that just arrived in newData
    latestTime: exec max time from newData;
    liveBook :: delete from liveBook where time < (latestTime - 0D00:00:02.000000000);
 };
