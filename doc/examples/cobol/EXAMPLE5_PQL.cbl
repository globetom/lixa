      * Copyright (c) 2009-2016, Christian Ferrari 
      * <tiian@users.sourceforge.net>
      * All rights reserved.
      *
      * This file is part of LIXA.
      *
      * LIXA is free software: you can redistribute it and/or modify
      * it under the terms of the GNU General Public License version 2i
      * as published by the Free Software Foundation.
      *
      * LIXA is distributed in the hope that it will be useful,
      * but WITHOUT ANY WARRANTY; without even the implied warranty of
      * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
      * GNU General Public License for more details.
      *
      * You should have received a copy of the 
      * GNU General Public License along with LIXA.
      * If not, see <http://www.gnu.org/licenses/>.
      *
        IDENTIFICATION DIVISION.
        PROGRAM-ID. EXAMPLE5-PQL.
        DATA DIVISION.
        WORKING-STORAGE SECTION.
      * Include TX definitions
        01 TX-RETURN-STATUS.
           COPY TXSTATUS.
        01 TX-INFO-AREA.
           COPY TXINFDEF.
      * Include LIXA definitions
           COPY LIXAXID.
      * PostgreSQL connection handle
        01 PGCONN USAGE POINTER.
      * PostgreSQL result
        01 PGRES USAGE POINTER.
        01 RESULT USAGE BINARY-LONG.
      * PostgreSQL statement
        01 PGSTATMNT PIC X(200) VALUE SPACES.
      * Command line argument
        01 ARGV PIC X(100) VALUE SPACES.
        01 ARGV-BOOL PIC 9(1) VALUE 0.
        88      IS-DELETE VALUES 1.
        88      IS-INSERT VALUES 0.
      *
        PROCEDURE DIVISION.
        000-MAIN.
            DISPLAY 'Executing EXAMPLE5_PQL'.
            MOVE ZERO TO TX-RETURN-STATUS.
      *
      * Delete or insert?
      *
            ACCEPT ARGV FROM ARGUMENT-VALUE.
      *     DISPLAY 'Passed argument is: "' ARGV '"'.
            IF ARGV IS EQUAL TO 'DELETE' OR
               ARGV IS EQUAL TO 'delete' THEN
               DISPLAY 'Deleting a row from the table...'
               MOVE 1 TO ARGV-BOOL
            ELSE
               DISPLAY 'Inserting a row from the table...'
            END-IF.
      *
      * Open the resource manager
      *
            CALL "TXOPEN" USING TX-RETURN-STATUS.
      *     DISPLAY 'TXOPEN returned value ' TX-STATUS.
            IF NOT TX-OK THEN
               DISPLAY 'Exiting...'
               STOP RUN RETURNING 1
            END-IF.
      *
      * Retrieve PostgreSQL connection
      * do NOT use standard PostgreSQL functions because they don't
      * return an XA connection, but a transaction manager independent
      * connection
      *
            CALL "LIXAPQGETCONN" RETURNING PGCONN.
            IF PGCONN EQUAL NULL THEN
               DISPLAY 'Error: unable to retrieve a valid PostgreSQL con
       -nection'
               STOP RUN RETURNING 1
            END-IF.
      *
      * Check connection status
      *
            CALL "PQstatus" USING BY VALUE PGCONN RETURNING RESULT.
            DISPLAY "Status: " RESULT.
      *         
      * Start a new transaction
      * 
            CALL "TXBEGIN" USING TX-RETURN-STATUS.
      *     DISPLAY 'TXBEGIN returned value ' TX-STATUS.
            IF NOT TX-OK THEN
               DISPLAY 'Exiting...'
               STOP RUN RETURNING 1
            END-IF.
      *
      * Prepare SQL statement
      *
            IF IS-DELETE THEN
      *
      * Execute DELETE stament
      *
               DISPLAY "PQexec DELETE" END-DISPLAY
               CALL "PQexec" USING
                 BY VALUE PGCONN
                 BY REFERENCE 
                    "DELETE FROM authors WHERE id=1;" & x"00"
                 RETURNING PGRES
               END-CALL
            ELSE
      *
      * Execute INSERT stament
      *
               DISPLAY "PQexec INSERT" END-DISPLAY
               CALL "PQexec" USING
                 BY VALUE PGCONN
                 BY REFERENCE 
                    "INSERT INTO authors VALUES(1,'Foo','Bar');" & x"00"
                 RETURNING PGRES
               END-CALL
            END-IF.
            DISPLAY 'PQexec return code: ' PGRES.
      *
      * Check connection status
      *
            CALL "PQstatus" USING BY VALUE PGCONN RETURNING RESULT.
            DISPLAY "Status: " RESULT.
      * Calling TXCOMMIT (tx_commit)
            CALL "TXCOMMIT" USING TX-RETURN-STATUS.
            DISPLAY 'TXCOMMIT returned value ' TX-STATUS.
            IF NOT TX-OK THEN
               DISPLAY 'Exiting...'
               STOP RUN RETURNING 1
            END-IF.
      * Calling TXCLOSE (tx_close)
            CALL "TXCLOSE" USING TX-RETURN-STATUS.
            DISPLAY 'TXCLOSE returned value ' TX-STATUS.
            IF NOT TX-OK
               STOP RUN RETURNING 1
            DISPLAY 'Execution terminated!'.
            STOP RUN RETURNING 0.
