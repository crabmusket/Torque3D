typedef union {
   Token< char >           c;
   Token< int >            i;
   Token< const char* >    s;
   Token< char* >          str;
   Token< double >         f;
   StmtNode*               stmt;
   ExprNode*               expr;
   SlotAssignNode*         slist;
   VarNode*                var;
   SlotDecl                slot;
   InternalSlotDecl        intslot;
   ObjectBlockDecl         odcl;
   ObjectDeclNode*         od;
   AssignDecl              asn;
   IfStmtNode*             ifnode;
} YYSTYPE;
#define	rwDEFINE	258
#define	rwCLIENTCMD	259
#define	rwSERVERCMD	260
#define	rwENDDEF	261
#define	rwDECLARE	262
#define	rwDECLARESINGLETON	263
#define	rwBREAK	264
#define	rwELSE	265
#define	rwCONTINUE	266
#define	rwGLOBAL	267
#define	rwIF	268
#define	rwNIL	269
#define	rwRETURN	270
#define	rwWHILE	271
#define	rwDO	272
#define	rwENDIF	273
#define	rwENDWHILE	274
#define	rwENDFOR	275
#define	rwDEFAULT	276
#define	rwFOR	277
#define	rwFOREACH	278
#define	rwFOREACHSTR	279
#define	rwIN	280
#define	rwDATABLOCK	281
#define	rwSWITCH	282
#define	rwCASE	283
#define	rwSWITCHSTR	284
#define	rwCASEOR	285
#define	rwPACKAGE	286
#define	rwNAMESPACE	287
#define	rwCLASS	288
#define	rwASSERT	289
#define	ILLEGAL_TOKEN	290
#define	CHRCONST	291
#define	INTCONST	292
#define	TTAG	293
#define	VAR	294
#define	IDENT	295
#define	TYPEIDENT	296
#define	DOCBLOCK	297
#define	STRATOM	298
#define	TAGATOM	299
#define	FLTCONST	300
#define	opINTNAME	301
#define	opINTNAMER	302
#define	opMINUSMINUS	303
#define	opPLUSPLUS	304
#define	STMT_SEP	305
#define	opSHL	306
#define	opSHR	307
#define	opPLASN	308
#define	opMIASN	309
#define	opMLASN	310
#define	opDVASN	311
#define	opMODASN	312
#define	opANDASN	313
#define	opXORASN	314
#define	opORASN	315
#define	opSLASN	316
#define	opSRASN	317
#define	opCAT	318
#define	opEQ	319
#define	opNE	320
#define	opGE	321
#define	opLE	322
#define	opAND	323
#define	opOR	324
#define	opSTREQ	325
#define	opCOLONCOLON	326
#define	opMDASN	327
#define	opNDASN	328
#define	opNTASN	329
#define	opSTRNE	330
#define	UNARY	331


extern YYSTYPE CMDlval;
