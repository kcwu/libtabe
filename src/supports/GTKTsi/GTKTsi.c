/*
 * Copyright 1999, TaBE Project, All Rights Reserved.
 * Copyright 1999, Pai-Hsiang Hsiao, All Rights Reserved.
 *
 * $Id: GTKTsi.c,v 1.2 2002/08/11 01:58:11 kcwu Exp $
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#include <gtk/gtk.h>

#include <tabe.h>

#include "bims.h"

/* signal handler for quit */
static void main_quit(GtkWidget *widget, gpointer data);
/* signal handler for next and prev */
static void browse_next(GtkWidget *widget, gpointer data);
static void browse_prev(GtkWidget *widget, gpointer data);
/* signal handler for search and set */
static void browse_set(GtkWidget *widget, gpointer data);
/* signal handler for edit */
static void browse_edit(GtkWidget *widget, gpointer data);
/* signal handler for add */
static void browse_add(GtkWidget *widget, gpointer data);
/* signal handler for delete */
static void browse_delete(GtkWidget *widget, gpointer data);
/* signal handler for selection done */
static void browse_selected(GtkWidget *widget,
                            gint row, gint column,
                            GdkEventButton *event, gpointer data);

/* signal handler for search_win quit */
static void search_quit(GtkWidget *widget, gpointer data);
/* signal handler for search_win done */
static void search_done(GtkWidget *widget, gpointer data);
static int  search_key_input(GtkWidget *widget, GdkEventKey *event);

/* signal handler for edit_win quit */
static void edit_quit(GtkWidget *widget, gpointer data);
/* signal handler for edit_win done */
static void edit_done(GtkWidget *widget, gpointer data);
/* signal handler for toggle button */
static void edit_toggled(GtkWidget *widget, gpointer data);

/* signal handler for file_win quit */
static void file_quit(GtkWidget *widget, gpointer data);
/* signal handler for file_win done */
static void file_done(GtkWidget *widget, gpointer data);

static void *tsiyin_realdump(void *data);

static void about_me(GtkWidget *widget, gpointer data);
static void about_done(GtkWidget *widget, gpointer data);

#define NUMBER_OF_CLIST_ROW    15
#define NUMBER_OF_CLIST_COLUMN 3

/* default Tsi DB name */
#define DEFAULT_TSI_DB_NAME "tsi.db"

/* this sucks */
struct gtktsi_data {
  GtkWidget        *clist;
  GtkWidget        *search_win, *search_entry;
  GtkWidget        *edit_win;
  GtkWidget        *file_win, *file_entry;
  GtkWidget        *dump_win;
  GtkWidget        *about_win;
  gchar            *tsi_data[NUMBER_OF_CLIST_ROW];
  gint              selected_row;
  gint              yinnum;
  Yin              *yindata;
  gint              pyinnum;
  Yin              *pyindata;
  unsigned long int yinarray;
  char             *db_name;
  struct TsiDB     *tsidb;
};

struct gtktsi_data *gtdata;

/*
 * check window mutual exclusive
 *
 * return 0 if ok to open, -1 fails
 */
int
win_mutual_exclusive()
{
  if (gtdata->search_win || gtdata->edit_win || gtdata->file_win ||
      gtdata->dump_win   || gtdata->about_win) {
    return(-1);
  }

  return(0);
}

enum {
  BROWSE_OPT_PREV,
  BROWSE_OPT_NEXT,
  BROWSE_OPT_SET
};

/*
 * the code sucks [FIXME: the unnature mapping of DB and clist]
 *
 * opt == BROWSE_OPT_PREV: prev tsi
 * opt == BROWSE_OPT_NEXT: next tsi
 * opt == BROWSE_OPT_SET: tsi set
 */
void
main_browse_tsi_in_clist(opt)
int opt;
{
  struct TsiInfo *tsi;
  ZhiStr zuyin;
  int i, j, k, rval, len;
  gchar *data[3];

  if (!gtdata->tsidb) {
    gtdata->tsidb = tabeTsiDBOpen(DB_TYPE_DB, gtdata->db_name, 0);
    if (!gtdata->tsidb) {
      return;
    }
  }

  tsi = (struct TsiInfo *)malloc(sizeof(struct TsiInfo));
  memset(tsi, 0, sizeof(struct TsiInfo));
  tsi->tsi = (unsigned char *)malloc(sizeof(unsigned char)*80);
  memset(tsi->tsi, 0, 80);

  for (i = 0; i < NUMBER_OF_CLIST_COLUMN; i++) {
    data[i] = (gchar *)malloc(sizeof(gchar)*200);
    memset(data[i], 0, 200);
  }

  gtk_clist_freeze(GTK_CLIST(gtdata->clist));

  if (gtdata->tsi_data[NUMBER_OF_CLIST_ROW-1]) {
    strcpy(tsi->tsi, gtdata->tsi_data[NUMBER_OF_CLIST_ROW-1]);
  }
  rval = gtdata->tsidb->CursorSet(gtdata->tsidb, tsi);
  if (rval < 0) {
    return;
  }
  gtk_clist_clear(GTK_CLIST(gtdata->clist));

  switch(opt) {
  case BROWSE_OPT_PREV: /* go back some tsi, or begin of file reached */
    for (i = 0; i < 2*NUMBER_OF_CLIST_ROW-1; i++) {
      rval = gtdata->tsidb->CursorPrev(gtdata->tsidb, tsi);
      if (rval < 0) {
	break;
      }
    }
    break;
  case BROWSE_OPT_NEXT:
    rval = gtdata->tsidb->CursorNext(gtdata->tsidb, tsi);
    break;
  case BROWSE_OPT_SET:
  /* we don't do anything else when opt == BROWSE_OPT_SET */
  default:
    break;
  }

  gtdata->tsi_data[0] = (gchar *)realloc(gtdata->tsi_data[0],
					 sizeof(gchar)*(strlen(tsi->tsi)+1));
  strcpy(gtdata->tsi_data[0], tsi->tsi);
  memset(data[0], 0, 200);
  strcpy(data[0], gtdata->tsi_data[0]);
  memset(data[1], 0, 200);
  sprintf(data[1], "%8ld", tsi->refcount);
  memset(data[2], 0, 200);

  len = strlen(tsi->tsi)/2;
  for (j = 0; j < tsi->yinnum; j++) {
    strcat(data[2], "[");
    for (k = 0; k < len; k++) {
      zuyin = tabeYinToZuYinSymbolSequence(tsi->yindata[j*len+k]);
      strcat(data[2], zuyin);
      strcat(data[2], " ");
      free(zuyin);
    }
    strcat(data[2], "] ");
  }

  gtk_clist_insert(GTK_CLIST(gtdata->clist), 0, data);

  for (i = 1; i < NUMBER_OF_CLIST_ROW; i++) {
    rval = gtdata->tsidb->CursorNext(gtdata->tsidb, tsi);
    if (rval < 0) {
      strcpy(gtdata->tsi_data[i], "");
      strcpy(data[0], gtdata->tsi_data[i]);
      strcpy(data[1], "");
    }
    else {
      gtdata->tsi_data[i] =
	(gchar *)realloc(gtdata->tsi_data[i],
			 sizeof(gchar)*(strlen(tsi->tsi)+1));
      strcpy(gtdata->tsi_data[i], tsi->tsi);
      strcpy(data[0], gtdata->tsi_data[i]);
      sprintf(data[1], "%8ld", tsi->refcount);
      memset(data[2], 0, 200);

      len = strlen(tsi->tsi)/2;
      for (j = 0; j < tsi->yinnum; j++) {
	strcat(data[2], "[");
	for (k = 0; k < len; k++) {
	  zuyin = tabeYinToZuYinSymbolSequence(tsi->yindata[j*len+k]);
	  strcat(data[2], zuyin);
	  strcat(data[2], " ");
	  free(zuyin);
	}
	strcat(data[2], "] ");
      }
    }
    gtk_clist_insert(GTK_CLIST(gtdata->clist), i, data);
  }

  /*
   * make sure next browse is ok even when there're not enough
   * tsi in this browse */
  strcpy(gtdata->tsi_data[NUMBER_OF_CLIST_ROW-1], tsi->tsi);

  gtk_clist_select_row(GTK_CLIST(gtdata->clist), gtdata->selected_row, 0);
  gtk_clist_thaw(GTK_CLIST(gtdata->clist));

  for (i = 0; i < NUMBER_OF_CLIST_COLUMN; i++) {
    free(data[i]);
  }
  if (tsi->yindata) {
    free(tsi->yindata);
    tsi->yindata = (Yin *)NULL;
  }
  if (tsi->tsi) {
    free(tsi->tsi);
  }
  free(tsi);
}

void
search_prompt_tsi_and_set()
{
  GtkWidget *vbox, *hbox;
  GtkWidget *done, *cancel;

  if (win_mutual_exclusive() < 0) {
    return;
  }

  if (gtdata->search_win) {
    gtk_widget_destroy(gtdata->search_win);
  }
  gtdata->search_win = gtk_window_new(GTK_WINDOW_DIALOG);
  gtk_container_set_border_width(GTK_CONTAINER(gtdata->search_win), 10);
  gtk_window_set_title(GTK_WINDOW(gtdata->search_win), "Search & Set");
  vbox = gtk_vbox_new(FALSE, 10);
  gtk_container_add(GTK_CONTAINER(gtdata->search_win), vbox);

  gtdata->search_entry = gtk_entry_new();
  gtk_signal_connect_after(GTK_OBJECT(gtdata->search_entry),
                           "key_press_event",
                           GTK_SIGNAL_FUNC(search_key_input), NULL);
  gtk_widget_show(gtdata->search_entry);
  gtk_box_pack_start(GTK_BOX(vbox), gtdata->search_entry, TRUE, TRUE, 0);

  hbox = gtk_hbox_new(TRUE, 10);
  done = gtk_button_new_with_label("搜尋");
  gtk_signal_connect(GTK_OBJECT(done), "clicked",
		     GTK_SIGNAL_FUNC(search_done), NULL);
  gtk_box_pack_start(GTK_BOX(hbox), done, TRUE, TRUE, 0);
  gtk_widget_show(done);
  cancel = gtk_button_new_with_label("取消");
  gtk_signal_connect(GTK_OBJECT(cancel), "clicked",
		     GTK_SIGNAL_FUNC(search_quit), NULL);
  gtk_box_pack_start(GTK_BOX(hbox), cancel, TRUE, TRUE, 0);
  gtk_widget_show(cancel);
  gtk_widget_show(hbox);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

  gtk_widget_show(vbox);
  gtk_widget_show(gtdata->search_win);
}

void
edit_tsi()
{
  GtkWidget *cbutton;
  GtkWidget *scr_win;
  GtkWidget *box, *hbox, *vbox;
  GtkWidget *done, *cancel;

  struct TsiInfo tsi;
  ZhiStr str = gtdata->tsi_data[gtdata->selected_row], zuyin, label;
  int len = strlen(str)/2;
  int rval, i, j;

  if (win_mutual_exclusive() < 0) {
    return;
  }

  memset(&tsi, 0, sizeof(tsi));
  tsi.tsi = strdup(str);
  rval = gtdata->tsidb->Get(gtdata->tsidb, &tsi);
  if (rval < 0) {
    fprintf(stderr, "Weird, no such Tsi.\n");
    return;
  }

  /* clear, clear, clear */
  gtdata->yinarray = 0;

  gtdata->yinnum = tsi.yinnum;
  if (gtdata->yindata) {
    free(gtdata->yindata);
    gtdata->yindata = (Yin *)NULL;
  }
  gtdata->yindata = (Yin *)malloc(sizeof(Yin)*len*gtdata->yinnum);
  memcpy(gtdata->yindata, tsi.yindata, sizeof(Yin)*len*gtdata->yinnum);

  tabeTsiInfoLookupPossibleTsiYin(&tsi);
  gtdata->pyinnum = tsi.yinnum;
  if (gtdata->pyindata) {
    free(gtdata->pyindata);
    gtdata->pyindata = (Yin *)NULL;
  }
  gtdata->pyindata = (Yin *)malloc(sizeof(Yin)*len*gtdata->pyinnum);
  memcpy(gtdata->pyindata, tsi.yindata, sizeof(Yin)*len*gtdata->pyinnum);

  if (gtdata->pyinnum > 64) {
    fprintf(stderr, "System limit in Yin reached.\n");
    return;
  }

  for (i = 0; i < gtdata->yinnum; i++) {
    for (j = 0; j < gtdata->pyinnum; j++) {
      if (!memcmp(gtdata->yindata+i*len, gtdata->pyindata+j*len,
		  sizeof(Yin)*len)) {
	gtdata->yinarray |= 1 << j;
      }
    }
  }

#ifdef MYDEBUG
  printf("Total %ld combinations.\n", tsi.yinnum);
  for (i = 0; i < gtdata->pyinnum; i++) {
    for (j = 0; j < len; j++) {
      printf("%s ",
	     tabeYinToZuYinSymbolSequence(gtdata->pyindata[i*len+j]));
    }
    printf("\n");
  }
#endif /* MYDEBUG */

  if (gtdata->edit_win) {
    gtk_widget_destroy(gtdata->edit_win);
  }
  gtdata->edit_win = gtk_window_new(GTK_WINDOW_DIALOG);
  gtk_container_set_border_width(GTK_CONTAINER(gtdata->edit_win), 10);
  gtk_window_set_title(GTK_WINDOW(gtdata->edit_win), "Edit");
  gtk_signal_connect(GTK_OBJECT(gtdata->edit_win), "destroy",
		     GTK_SIGNAL_FUNC(edit_quit), NULL);
  box = gtk_vbox_new(FALSE, 10);
  gtk_container_add(GTK_CONTAINER(gtdata->edit_win), box);
  scr_win = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scr_win),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start(GTK_BOX(box), scr_win, TRUE, TRUE, 0);
  gtk_widget_set_usize(scr_win, 400, 400);
  gtk_widget_show(scr_win);

  vbox = gtk_vbox_new(TRUE, 10);
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scr_win), vbox);
  hbox = gtk_hbox_new(TRUE, 10);
  gtk_box_pack_start(GTK_BOX(box), hbox, TRUE, TRUE, 0);

  zuyin = (ZhiStr)NULL;
  for (i = 0; i < gtdata->pyinnum; i++) {
    label = (ZhiStr)malloc(sizeof(unsigned char));
    strcpy(label, "");
    for (j = 0; j < len; j++) {
      zuyin = tabeYinToZuYinSymbolSequence(gtdata->pyindata[i*len+j]);
      label = (ZhiStr)realloc(label, strlen(label)+strlen(zuyin)+4);
      strcat(label, "[");
      strcat(label, zuyin);
      strcat(label, "] ");
    }
    cbutton = gtk_check_button_new_with_label(label);
    if (gtdata->yinarray & 1 << i) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cbutton), TRUE);
    }
    gtk_signal_connect(GTK_OBJECT(cbutton), "clicked",
		       GTK_SIGNAL_FUNC(edit_toggled), (gpointer)i);
    gtk_box_pack_start(GTK_BOX(vbox), cbutton, FALSE, FALSE, 0);
    gtk_widget_show(cbutton);
    free(zuyin);
    free(label);
  }

  done = gtk_button_new_with_label("設定");
  gtk_box_pack_start(GTK_BOX(hbox), done, TRUE, TRUE, 0);
  gtk_signal_connect(GTK_OBJECT(done), "clicked",
		     GTK_SIGNAL_FUNC(edit_done), NULL);
  gtk_widget_show(done);
  cancel = gtk_button_new_with_label("取消");
  gtk_signal_connect(GTK_OBJECT(cancel), "clicked",
		     GTK_SIGNAL_FUNC(edit_quit), NULL);
  gtk_box_pack_start(GTK_BOX(hbox), cancel, TRUE, TRUE, 0);
  gtk_widget_show(cancel);

  gtk_widget_show(vbox);
  gtk_widget_show(hbox);
  gtk_widget_show(box);
  gtk_widget_show(gtdata->edit_win);
}

void
file_open_db()
{
  GtkWidget *hbox, *vbox;
  GtkWidget *done, *cancel;

  if (win_mutual_exclusive() < 0) {
    return;
  }

  if (gtdata->file_win) {
    gtk_widget_destroy(gtdata->file_win);
  }
  gtdata->file_win = gtk_window_new(GTK_WINDOW_DIALOG);
  gtk_container_set_border_width(GTK_CONTAINER(gtdata->file_win), 10);
  gtk_window_set_title(GTK_WINDOW(gtdata->file_win), "File");
  gtk_signal_connect(GTK_OBJECT(gtdata->file_win), "destroy",
		     GTK_SIGNAL_FUNC(file_quit), NULL);
  vbox = gtk_vbox_new(FALSE, 10);
  gtk_container_add(GTK_CONTAINER(gtdata->file_win), vbox);

  gtdata->file_entry = gtk_entry_new();
  gtk_widget_show(gtdata->file_entry);
  gtk_box_pack_start(GTK_BOX(vbox), gtdata->file_entry, TRUE, TRUE, 0);

  hbox = gtk_hbox_new(TRUE, 10);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
  done = gtk_button_new_with_label("開啟");
  gtk_box_pack_start(GTK_BOX(hbox), done, TRUE, TRUE, 0);
  gtk_signal_connect(GTK_OBJECT(done), "clicked",
		     GTK_SIGNAL_FUNC(file_done), NULL);
  gtk_widget_show(done);
  cancel = gtk_button_new_with_label("取消");
  gtk_signal_connect(GTK_OBJECT(cancel), "clicked",
		     GTK_SIGNAL_FUNC(file_quit), NULL);
  gtk_box_pack_start(GTK_BOX(hbox), cancel, TRUE, TRUE, 0);
  gtk_widget_show(cancel);

  gtk_widget_show(vbox);
  gtk_widget_show(hbox);
  gtk_widget_show(gtdata->file_win);
}

/*
 * dump tsiyin db
 *
 * we did not dump in this function, but set up a timeout to
 * call tsiyin_realdump()
 */
void
tsiyin_dump(widget, data)
GtkWidget *widget;
gpointer data;
{
  GtkWidget *pbar;
  pthread_t thread;

  if (win_mutual_exclusive() < 0) {
    return;
  }

  gtdata->dump_win = gtk_window_new(GTK_WINDOW_DIALOG);
  gtk_container_set_border_width(GTK_CONTAINER(gtdata->dump_win), 10);
  gtk_window_set_title(GTK_WINDOW(gtdata->dump_win), "TsiYin Dump");
  pbar = gtk_progress_bar_new();
  gtk_progress_set_show_text(GTK_PROGRESS(pbar), 1);
  gtk_container_add(GTK_CONTAINER(gtdata->dump_win), pbar);
  gtk_widget_show(pbar);
  gtk_widget_show(gtdata->dump_win);

  pthread_create(&thread, NULL, tsiyin_realdump, pbar);
}

/* This is the GtkItemFactoryEntry structure used to generate new menus.
   Item 1: The menu path. The letter after the underscore indicates an
   accelerator key once the menu is open.
   Item 2: The accelerator key for the entry
   Item 3: The callback function.
   Item 4: The callback action.  This changes the parameters with
   which the function is called.  The default is 0.
   Item 5: The item type, used to define what kind of an item it is.
   Here are the possible values:
   
   NULL               -> "<Item>"
   ""                 -> "<Item>"
   "<Title>"          -> create a title item
   "<Item>"           -> create a simple item
   "<CheckItem>"      -> create a check item
   "<ToggleItem>"     -> create a toggle item
   "<RadioItem>"      -> create a radio item
   <path>             -> path of a radio item to link against
   "<Separator>"      -> create a separator
   "<Branch>"         -> create an item to hold sub items
   "<LastBranch>"     -> create a right justified branch 
*/

static GtkItemFactoryEntry menu_items[] = {
  {"/詞庫",            NULL,         NULL,          0, "<Branch>"},
  {"/詞庫/開啟",       "<control>O", file_open_db,  0, NULL},
  {"/詞庫/sep1",       NULL,         NULL,          0, "<Separator>"},
  {"/詞庫/結束",       "<control>Q", main_quit,     0, NULL},
  {"/工具",            NULL,         NULL,          0, "<Branch>"},
  {"/工具/上一頁",     "<control>P", browse_prev,   0, NULL},
  {"/工具/下一頁",     "<control>N", browse_next,   0, NULL},
  {"/工具/sep1",       NULL,         NULL,          0, "<Separator>"},
  {"/工具/搜尋",       "<control>S", browse_set,    0, NULL},
  {"/修改",            NULL,         NULL,          0, "<Branch>"},
  {"/修改/編輯",       "<control>E", browse_edit,   0, NULL},
  {"/修改/刪除",       "<control>D", browse_delete, 0, NULL},
  {"/修改/新增",       "<control>A", browse_add,    0, NULL},
  {"/詞音/產生讀音庫", "<control>Y", tsiyin_dump,   0, NULL},
  {"/資訊",            NULL,         NULL,          0, "<LastBranch>"},
  {"/資訊/有關本軟體", NULL,         about_me,      0, NULL},
};

void
get_main_menu(window, menubar)
GtkWidget *window;
GtkWidget **menubar;
{
  int nmenu_items = sizeof(menu_items)/sizeof(menu_items[0]);
  GtkItemFactory *item_factory;
  GtkAccelGroup *accel_group;
  
  accel_group = gtk_accel_group_new();
  
  /* This function initializes the item factory.
     Param 1: The type of menu - can be GTK_TYPE_MENU_BAR, GTK_TYPE_MENU,
     or GTK_TYPE_OPTION_MENU.
     Param 2: The path of the menu.
     Param 3: A pointer to a gtk_accel_group.  The item factory sets up
     the accelerator table while generating menus.
  */
  
  item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<main>", 
				      accel_group);
  
  /* This function generates the menu items. Pass the item factory,
     the number of items in the array, the array itself, and any
     callback data for the the menu items. */
  gtk_item_factory_create_items(item_factory, nmenu_items, menu_items, NULL);
  
  /* Attach the new accelerator group to the window. */
  gtk_accel_group_attach(accel_group, GTK_OBJECT (window));
  
  if (menubar)
    /* Finally, return the actual menu bar created by the item factory. */ 
    *menubar = gtk_item_factory_get_widget(item_factory, "<main>");
}

static gchar *clist_title[NUMBER_OF_CLIST_COLUMN] = {
  "詞",
  "參考次數",
  "讀音"
};

void
get_main_clist(clist)
GtkWidget **clist;
{
  GtkWidget *widget;

  widget = gtk_clist_new_with_titles(NUMBER_OF_CLIST_COLUMN, clist_title);
  gtk_clist_set_shadow_type(GTK_CLIST(widget), GTK_SHADOW_OUT);
  gtk_clist_set_column_width(GTK_CLIST(widget), 0, 200);
  gtk_clist_set_column_width(GTK_CLIST(widget), 2, 400);
  gtk_signal_connect(GTK_OBJECT(widget), "select_row",
		     GTK_SIGNAL_FUNC(browse_selected), NULL);

  *clist = widget;
}

int
main(argc, argv)
int argc;
char **argv;
{
  GtkWidget *window;
  GtkWidget *main_vbox;
  GtkWidget *menubar;
  GtkWidget *clist;
  
  gdk_set_locale();
  gtk_init (&argc, &argv);
  gtk_rc_parse("GTKTsi.rc");

  bimsInit("tsi.db", "yin.db");

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_signal_connect(GTK_OBJECT(window), "destroy", 
		     GTK_SIGNAL_FUNC(main_quit), 
		     "WM destroy");
  gtk_window_set_title(GTK_WINDOW(window), "GTKTsi");
  
  main_vbox = gtk_vbox_new(FALSE, 1);
  gtk_container_border_width(GTK_CONTAINER(main_vbox), 1);
  gtk_container_add(GTK_CONTAINER(window), main_vbox);
  gtk_widget_show(main_vbox);
  
  get_main_menu(window, &menubar);
  gtk_box_pack_start(GTK_BOX(main_vbox), menubar, FALSE, TRUE, 0);
  gtk_widget_show(menubar);

  get_main_clist(&clist);
  gtk_box_pack_start(GTK_BOX(main_vbox), clist, FALSE, TRUE, 0);
  gtk_widget_show(clist);
  
  gtdata = (struct gtktsi_data *)malloc(sizeof(struct gtktsi_data));
  memset(gtdata, 0, sizeof(struct gtktsi_data));
  gtdata->db_name = DEFAULT_TSI_DB_NAME;
  gtdata->clist = clist;
  main_browse_tsi_in_clist(BROWSE_OPT_SET);

  gtk_widget_show(window);
  gtk_main();

  bimsDestroy();
  
  return(0);
}

/*
 * simply quit the program
 */
static void
main_quit(widget, data)
GtkWidget *widget;
gpointer data;
{
  if (gtdata->tsidb) {
    gtdata->tsidb->Close(gtdata->tsidb);
  }
  gtk_main_quit();
}

static void
browse_next(widget, data)
GtkWidget *widget;
gpointer data;
{
  if (gtdata->tsidb) {
    main_browse_tsi_in_clist(BROWSE_OPT_NEXT);
  }
}

static void
browse_prev(widget, data)
GtkWidget *widget;
gpointer data;
{
  if (gtdata->tsidb) {
    main_browse_tsi_in_clist(BROWSE_OPT_PREV);
  }
}

static void
browse_set(widget, data)
GtkWidget *widget;
gpointer data;
{
  search_prompt_tsi_and_set(gtdata);
}

static void
browse_edit(widget, data)
GtkWidget *widget;
gpointer data;
{
#ifdef MYDEBUG
  printf("selected word is %s\n", gtdata->tsi_data[gtdata->selected_row]);
#endif
  edit_tsi(gtdata);
}

static void
browse_add(widget, data)
GtkWidget *widget;
gpointer data;
{

}

#include <db.h>

static void
browse_delete(widget, data)
GtkWidget *widget;
gpointer data;
{
  DBT key;
  int rval;

  memset(&key, 0, sizeof(DBT));
  printf("%s\n", gtdata->tsi_data[gtdata->selected_row]);
  key.data = gtdata->tsi_data[gtdata->selected_row];
  key.size = strlen(gtdata->tsi_data[gtdata->selected_row]);
  rval = ((DB *)(gtdata->tsidb->dbp))->del((DB *)gtdata->tsidb->dbp,
                                           NULL, &key, 0);
  if (gtdata->selected_row == 0) {
    strcpy(gtdata->tsi_data[0],
           gtdata->tsi_data[1]);
  }
  strcpy(gtdata->tsi_data[NUMBER_OF_CLIST_ROW-1], gtdata->tsi_data[0]);
  main_browse_tsi_in_clist(BROWSE_OPT_SET);
}

static void
browse_selected(widget, row, column, event, data)
GtkWidget *widget;
gint row;
gint column;
GdkEventButton *event;
gpointer data;
{
  gtdata->selected_row = row;
}

static int
search_key_input(widget, event)
GtkWidget *widget;
GdkEventKey *event;
{
  return(bimsGTKEntryFeedKey(widget, event));
}

static void
search_quit(widget, data)
GtkWidget *widget;
gpointer data;
{
  gtk_widget_destroy(GTK_WIDGET(gtdata->search_win));
  gtdata->search_win = (GtkWidget *)NULL;
  gtdata->search_entry = (GtkWidget *)NULL;
}

static void
search_done(widget, data)
GtkWidget *widget;
gpointer data;
{
  struct TsiInfo *tsi;
  unsigned char *str;
  int rval;

  tsi = (struct TsiInfo *)malloc(sizeof(struct TsiInfo));
  memset(tsi, 0, sizeof(struct TsiInfo));
  tsi->tsi = (unsigned char *)malloc(sizeof(unsigned char)*80);

  str = gtk_entry_get_text(GTK_ENTRY(gtdata->search_entry));
  strcpy(tsi->tsi, str);

  rval = gtdata->tsidb->Get(gtdata->tsidb, tsi);
  if (rval < 0) {
    gtk_entry_set_text(GTK_ENTRY(gtdata->search_entry), "無此詞");
  }
  else {
    gtk_widget_destroy(GTK_WIDGET(gtdata->search_win));
    gtdata->search_win = (GtkWidget *)NULL;
    gtdata->search_entry = (GtkWidget *)NULL;
    gtdata->tsi_data[NUMBER_OF_CLIST_ROW-1] =
      (gchar *)realloc(gtdata->tsi_data[NUMBER_OF_CLIST_ROW-1],
		       sizeof(gchar)*(strlen(tsi->tsi)+1));
    strcpy(gtdata->tsi_data[NUMBER_OF_CLIST_ROW-1], tsi->tsi);
    main_browse_tsi_in_clist(BROWSE_OPT_SET);
  }
}

static void
edit_done(widget, data)
GtkWidget *widget;
gpointer data;
{
  unsigned long int yinarray;
  Yin *yindata;
  ZhiStr str = gtdata->tsi_data[gtdata->selected_row];
  struct TsiInfo tsi;
  int len = strlen(str)/2;
  int i, j, num, rval, index;

  yinarray = 0;
  yindata = (Yin *)NULL;

  for (i = 0; i < gtdata->yinnum; i++) {
    for (j = 0; j < gtdata->pyinnum; j++) {
      if (!memcmp(gtdata->yindata+i*len, gtdata->pyindata+j*len,
		  sizeof(Yin)*len)) {
	yinarray |= 1 << j;
      }
    }
  }

  if (yinarray != gtdata->yinarray) { /* modified, need save */
    num = 0;
    index = 0;
    for (i = gtdata->yinarray; i > 0; i = i >> 1, index++) {
      if (i & 0x1) {
	yindata = (Yin *)realloc(yindata, sizeof(Yin)*len*(num+1));
	memcpy(yindata+num*len, gtdata->pyindata+index*len,
	       sizeof(Yin)*len);
	num++;
      }
    }
    if (gtdata->yindata) {
      free(gtdata->yindata);
      gtdata->yindata = (Yin *)NULL;
    }
    gtdata->yindata = yindata;
    gtdata->yinnum  = num;
    memset(&tsi, 0, sizeof(tsi));
    tsi.tsi = str;
    rval = gtdata->tsidb->Get(gtdata->tsidb, &tsi);
    if (rval < 0) {
      fprintf(stderr, "Weird, this is the editing session, not adding one!\n");
      return;
    }
    tsi.yinnum = num;
    tsi.yindata = yindata;
    (gtdata->tsidb)->flags |= DB_FLAG_OVERWRITE;
    gtdata->tsidb->Put(gtdata->tsidb, &tsi);
    (gtdata->tsidb)->flags ^= DB_FLAG_OVERWRITE;
    /*
     * we are doing wrong here,
     * but, we don't expect Tsi DB to have a sync interface
     */
    gtdata->tsidb->Close(gtdata->tsidb);
    gtdata->tsidb = (struct TsiDB *)NULL;
    gtdata->tsi_data[NUMBER_OF_CLIST_ROW-1] =
      (ZhiStr)realloc(gtdata->tsi_data[NUMBER_OF_CLIST_ROW-1],
		      sizeof(unsigned char)*(strlen(gtdata->tsi_data[0])+1));
    strcpy(gtdata->tsi_data[NUMBER_OF_CLIST_ROW-1], gtdata->tsi_data[0]);
    main_browse_tsi_in_clist(BROWSE_OPT_SET);
  }
  else {
    /* do nothing */
  }
  gtk_widget_destroy(GTK_WIDGET(gtdata->edit_win));
  gtdata->edit_win = (GtkWidget *)NULL;
}

static void
edit_quit(widget, data)
GtkWidget *widget;
gpointer data;
{
  gtk_widget_destroy(GTK_WIDGET(gtdata->edit_win));
  gtdata->edit_win = (GtkWidget *)NULL;
}

static void
edit_toggled(widget, data)
GtkWidget *widget;
gpointer data;
{
  int index = (gint)data;

  gtdata->yinarray ^= 1 << index;
}

static void
file_quit(widget, data)
GtkWidget *widget;
gpointer data;
{
  gtk_widget_destroy(GTK_WIDGET(gtdata->file_win));
  gtdata->file_win = (GtkWidget *)NULL;
}

static void
file_done(widget, data)
GtkWidget *widget;
gpointer data;
{
  struct TsiDB *tsidb;
  unsigned char *str;

  str = gtk_entry_get_text(GTK_ENTRY(gtdata->file_entry));

  gtdata->db_name = strdup(str);
  tsidb = tabeTsiDBOpen(DB_TYPE_DB, gtdata->db_name, 0);
  if (!tsidb) {
    gtk_entry_set_text(GTK_ENTRY(gtdata->file_entry), "無此詞庫");
  }
  else {
    gtk_widget_destroy(GTK_WIDGET(gtdata->file_win));
    gtdata->file_win = (GtkWidget *)NULL;
    gtdata->file_entry = (GtkWidget *)NULL;
    gtdata->tsidb = tsidb;
    main_browse_tsi_in_clist(BROWSE_OPT_SET);
  }
}

double ratio;

static int
tsiyin_dumpupdate(data)
void *data;
{
  GtkWidget *pbar = GTK_WIDGET(data);

  gtk_progress_bar_update(GTK_PROGRESS_BAR(pbar), ratio);

  return(TRUE);
}

static void *
tsiyin_realdump(data)
void *data;
{
  struct TsiDB *tdb;
  struct TsiYinDB *ydb;
  struct TsiInfo *tsi;
  struct TsiYinInfo *tsiyin;
  char *ydb_name = "yin.db";
  int rval, i, j, len;
  int total;
  int tag;

  tdb = tabeTsiDBOpen(DB_TYPE_DB, gtdata->db_name, 0);
  if (!tdb) {
    printf("Error: could not open Tsi db %s.\n", gtdata->db_name);
    return(FALSE);
  }
  total = tdb->RecordNumber(tdb);

  ydb = tabeTsiYinDBOpen(DB_TYPE_DB, ydb_name,
                         DB_FLAG_CREATEDB|DB_FLAG_OVERWRITE);
  if (!ydb) {
    printf("Error: could not open Yin db %s.\n", ydb_name);
    return(FALSE);
  }

  tsi = (struct TsiInfo *)malloc(sizeof(struct TsiInfo));
  memset(tsi, 0, sizeof(struct TsiInfo));
  tsi->tsi = (unsigned char *)malloc(sizeof(unsigned char)*80);
  memset(tsi->tsi, 0, 80);

  tsiyin = (struct TsiYinInfo *)malloc(sizeof(struct TsiYinInfo));
  memset(tsiyin, 0, sizeof(struct TsiYinInfo));

  tag = gtk_timeout_add(500, tsiyin_dumpupdate, data);

  i = 0;
  while(1) {
    if (i == 0) {
      tdb->CursorSet(tdb, tsi);
    }
    else {
      rval = tdb->CursorNext(tdb, tsi);
      if (rval < 0) {
	break;
      }
    }
    i++;
    if (!(i%100)) {
      ratio = (double)i/total;
    }
    tabeTsiInfoLookupPossibleTsiYin(tsi);
    len = strlen(tsi->tsi)/2;
    for (j = 0; j < tsi->yinnum; j++) {
      tsiyin->yinlen = len;
      tsiyin->yin = (Yin *)malloc(sizeof(Yin)*len);
      memcpy(tsiyin->yin, tsi->yindata+j*len, sizeof(Yin)*len);
      rval = ydb->Get(ydb, tsiyin);
      if (rval < 0) { /* no such tsiyin */
        tsiyin->tsinum = 1;
        tsiyin->tsidata = (ZhiStr)malloc(sizeof(unsigned char)*len*2);
        memcpy(tsiyin->tsidata, tsi->tsi, sizeof(unsigned char)*len*2);
        ydb->Put(ydb, tsiyin);
      }
      else {
        tsiyin->tsidata =
          (ZhiStr)realloc(tsiyin->tsidata,
			  sizeof(unsigned char)*((tsiyin->tsinum+1)*len*2));
        memcpy(tsiyin->tsidata+(tsiyin->tsinum*len*2), tsi->tsi,
               sizeof(unsigned char)*len*2);
        tsiyin->tsinum++;
        ydb->Put(ydb, tsiyin);
      }
    }
  }

  tdb->Close(tdb);
  ydb->Close(ydb);
  gtk_timeout_remove(tag);
  gtk_widget_destroy(gtdata->dump_win);
  gtdata->dump_win = (GtkWidget *)NULL;

  pthread_exit(0);

  return(NULL);
}

static char *readme =
  "GTKTsi is a part of TaBE Project.\n"
  "Copyright 1999, TaBE Project. All Rights Reserved.\n"
  "There're %6ld tsi in the database.";

static void
about_me(widget, data)
GtkWidget *widget;
gpointer data;
{
  GtkWidget *vbox;
  GtkWidget *label, *done;
  char *buf;

  buf = (char *)malloc(sizeof(char)*(strlen(readme)+6));
  sprintf(buf, readme, gtdata->tsidb->RecordNumber(gtdata->tsidb));
  gtdata->about_win = gtk_window_new(GTK_WINDOW_DIALOG);
  gtk_window_set_title(GTK_WINDOW(gtdata->about_win), "About GTKTsi");
  gtk_container_border_width(GTK_CONTAINER(gtdata->about_win), 10);
  gtk_signal_connect(GTK_OBJECT(gtdata->about_win), "destroy", 
		     GTK_SIGNAL_FUNC(about_done), NULL);
  vbox = gtk_vbox_new(FALSE, 10);
  gtk_container_add(GTK_CONTAINER(gtdata->about_win), vbox);

  label = gtk_label_new(buf);
  gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
  gtk_widget_show(label);

  done = gtk_button_new_with_label("關閉");
  gtk_signal_connect(GTK_OBJECT(done), "clicked", 
		     GTK_SIGNAL_FUNC(about_done), NULL);
  gtk_box_pack_start(GTK_BOX(vbox), done, FALSE, FALSE, 0);
  gtk_widget_show(done);

  gtk_widget_show(vbox);
  gtk_widget_show(gtdata->about_win);

  free(buf);
}

static void
about_done(widget, data)
GtkWidget *widget;
gpointer data;
{
  gtk_widget_destroy(gtdata->about_win);
  gtdata->about_win = (GtkWidget *)NULL;
}
