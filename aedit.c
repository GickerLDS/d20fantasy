/*
 * File: aedit.c
 * Comment: OLC for MUDs -- this one edits socials
 * by Michael Scott <scottm@workcomm.net> -- 06/10/96
 * for use with OasisOLC
 * ftpable from ftp.circlemud.org:/pub/CircleMUD/contrib/code
 */

#include "conf.h"
#include "sysdep.h"

SVNHEADER("$Id: aedit.c 60 2009-03-25 06:38:01Z pladow $");

#include "structs.h"
#include "interpreter.h"
#include "handler.h"
#include "comm.h"
#include "utils.h"
#include "db.h"
#include "oasis.h"
#include "screen.h"
#include "constants.h"
#include "genolc.h"

/* external functs */
int sort_command_helper(const void *a, const void *b);
void sort_commands(void); /* aedit patch -- M. Scott */
void create_command_list(void);
void free_action(struct social_messg *action);

/* function protos */
void aedit_disp_menu(struct descriptor_data * d);
void aedit_parse(struct descriptor_data * d, char *arg);
void aedit_setup_new(struct descriptor_data *d);
void aedit_setup_existing(struct descriptor_data *d, int real_num);
void aedit_save_to_disk(struct descriptor_data *d);
void aedit_save_internally(struct descriptor_data *d);
int aedit_find_command(const char *txt);

/*
 * Utils and exported functions.
 */

/*------------------------------------------------------------------------*\
  Utils and exported functions.
\*------------------------------------------------------------------------*/

ACMD(do_oasis_aedit)
{
  char arg[MAX_INPUT_LENGTH];
  struct descriptor_data *d;
  int i;
  
  if (CONFIG_NEW_SOCIALS == 0) {
    send_to_char(ch, "Socials cannot be edited at the moment.\r\n");
    return;
  }

  if (GET_OLC_ZONE(ch) != AEDIT_PERMISSION && GET_ADMLEVEL(ch) < ADMLVL_IMPL) {
    send_to_char(ch, "You don't have access to editing socials.\r\n");
    return;
  }

  for (d = descriptor_list; d; d = d->next)
    if (STATE(d) == CON_AEDIT) {
      send_to_char(ch, "Sorry, only one can edit socials at a time.\r\n");
      return;
    }

  one_argument(argument, arg);
    
  if (!*arg) {
    send_to_char(ch, "Please specify a social to edit.\r\n");
    return;
  }

  d = ch->desc;

  if (!str_cmp("save", arg)) {
    mudlog(CMP, MAX(ADMLVL_BUILDER, GET_INVIS_LEV(ch)), TRUE, "OLC: %s saves socials.", GET_NAME(ch));
    send_to_char(ch, "Writing social file..\r\n");
    aedit_save_to_disk(d);
    send_to_char(ch, "Done.\r\n");
    return;
  }
  
  /*
   * Give descriptor an OLC structure.
   */
  if (d->olc) {
    mudlog(BRF, ADMLVL_IMMORT, TRUE, "SYSERR: do_oasis: Player already had olc structure.");
    free(d->olc);
  }
  CREATE(d->olc, struct oasis_olc_data, 1);

  OLC_NUM(d) = 0;
  OLC_STORAGE(d) = strdup(arg);
  
  for (OLC_ZNUM(d) = 0; (OLC_ZNUM(d) <= top_of_socialt); OLC_ZNUM(d)++)  
    if (is_abbrev(OLC_STORAGE(d), soc_mess_list[OLC_ZNUM(d)].command))
      break;

  if (OLC_ZNUM(d) > top_of_socialt)  {
    if ((i = aedit_find_command(OLC_STORAGE(d))) != -1)  {
      send_to_char(ch, "The '%s' command already exists (%s).\r\n", OLC_STORAGE(d), complete_cmd_info[i].command);
      cleanup_olc(d, CLEANUP_ALL);
      return;
    }
    send_to_char(ch, "Do you wish to add the '%s' action? ", OLC_STORAGE(d));
    OLC_MODE(d) = AEDIT_CONFIRM_ADD;
  } else {
    send_to_char(ch, "Do you wish to edit the '%s' action? ", soc_mess_list[OLC_ZNUM(d)].command);
    OLC_MODE(d) = AEDIT_CONFIRM_EDIT;
  }
  STATE(d) = CON_AEDIT;
  act("$n starts using OLC.", TRUE, d->character, 0, 0, TO_ROOM);
  SET_BIT_AR(PLR_FLAGS(ch), PLR_WRITING);
  mudlog(CMP, ADMLVL_IMMORT, TRUE, "OLC: %s starts editing actions.", GET_NAME(ch));
}


void aedit_setup_new(struct descriptor_data *d) {
   CREATE(OLC_ACTION(d), struct social_messg, 1);
   OLC_ACTION(d)->command             = strdup(OLC_STORAGE(d));
   OLC_ACTION(d)->sort_as             = strdup(OLC_STORAGE(d));
   OLC_ACTION(d)->hide                = 0;
   OLC_ACTION(d)->min_victim_position = POS_STANDING;
   OLC_ACTION(d)->min_char_position   = POS_STANDING;
   OLC_ACTION(d)->min_level_char      = 0;
   OLC_ACTION(d)->char_no_arg         = strdup("This action is unfinished.");
   OLC_ACTION(d)->others_no_arg       = strdup("This action is unfinished.");
   OLC_ACTION(d)->char_found          = NULL;
   OLC_ACTION(d)->others_found        = NULL;
   OLC_ACTION(d)->vict_found          = NULL;
   OLC_ACTION(d)->not_found           = NULL;
   OLC_ACTION(d)->char_auto           = NULL;
   OLC_ACTION(d)->others_auto         = NULL;
   OLC_ACTION(d)->char_body_found     = NULL;
   OLC_ACTION(d)->others_body_found   = NULL;
   OLC_ACTION(d)->vict_body_found     = NULL;
   OLC_ACTION(d)->char_obj_found      = NULL;
   OLC_ACTION(d)->others_obj_found    = NULL;
   aedit_disp_menu(d);
   OLC_VAL(d) = 0;
}

/*------------------------------------------------------------------------*/

void aedit_setup_existing(struct descriptor_data *d, int real_num) {
   CREATE(OLC_ACTION(d), struct social_messg, 1);
   OLC_ACTION(d)->command             = strdup(soc_mess_list[real_num].command);
   OLC_ACTION(d)->sort_as             = strdup(soc_mess_list[real_num].sort_as);
   OLC_ACTION(d)->hide                = soc_mess_list[real_num].hide;
   OLC_ACTION(d)->min_victim_position = soc_mess_list[real_num].min_victim_position;
   OLC_ACTION(d)->min_char_position   = soc_mess_list[real_num].min_char_position;
   OLC_ACTION(d)->min_level_char      = soc_mess_list[real_num].min_level_char;
   if (soc_mess_list[real_num].char_no_arg)
     OLC_ACTION(d)->char_no_arg       = strdup(soc_mess_list[real_num].char_no_arg);
   if (soc_mess_list[real_num].others_no_arg)
     OLC_ACTION(d)->others_no_arg     = strdup(soc_mess_list[real_num].others_no_arg);
   if (soc_mess_list[real_num].char_found)
     OLC_ACTION(d)->char_found        = strdup(soc_mess_list[real_num].char_found);
   if (soc_mess_list[real_num].others_found)
     OLC_ACTION(d)->others_found      = strdup(soc_mess_list[real_num].others_found);
   if (soc_mess_list[real_num].vict_found)
     OLC_ACTION(d)->vict_found        = strdup(soc_mess_list[real_num].vict_found);
   if (soc_mess_list[real_num].not_found)
     OLC_ACTION(d)->not_found         = strdup(soc_mess_list[real_num].not_found);
   if (soc_mess_list[real_num].char_auto)
     OLC_ACTION(d)->char_auto         = strdup(soc_mess_list[real_num].char_auto);
   if (soc_mess_list[real_num].others_auto)
     OLC_ACTION(d)->others_auto       = strdup(soc_mess_list[real_num].others_auto);
   if (soc_mess_list[real_num].char_body_found)
     OLC_ACTION(d)->char_body_found   = strdup(soc_mess_list[real_num].char_body_found);
   if (soc_mess_list[real_num].others_body_found)
     OLC_ACTION(d)->others_body_found = strdup(soc_mess_list[real_num].others_body_found);
   if (soc_mess_list[real_num].vict_body_found)
     OLC_ACTION(d)->vict_body_found   = strdup(soc_mess_list[real_num].vict_body_found);
   if (soc_mess_list[real_num].char_obj_found)
     OLC_ACTION(d)->char_obj_found    = strdup(soc_mess_list[real_num].char_obj_found);
   if (soc_mess_list[real_num].others_obj_found)
     OLC_ACTION(d)->others_obj_found  = strdup(soc_mess_list[real_num].others_obj_found);
   OLC_VAL(d) = 0;
   aedit_disp_menu(d);
}


      
void aedit_save_internally(struct descriptor_data *d) {
   struct social_messg *new_soc_mess_list = NULL;
   int i;
   
   /* add a new social into the list */
   if (OLC_ZNUM(d) > top_of_socialt)  {
      CREATE(new_soc_mess_list, struct social_messg, top_of_socialt + 2);
      for (i = 0; i <= top_of_socialt; i++)
              new_soc_mess_list[i] = soc_mess_list[i];
      new_soc_mess_list[++top_of_socialt] = *OLC_ACTION(d);
      free(soc_mess_list);
      soc_mess_list = new_soc_mess_list;
   }
   /* pass the editted action back to the list - no need to add */
   else {
      i = aedit_find_command(OLC_ACTION(d)->command);
      OLC_ACTION(d)->act_nr = soc_mess_list[OLC_ZNUM(d)].act_nr;
      /* why did i do this..? hrm */
      free_action(soc_mess_list + OLC_ZNUM(d));
      soc_mess_list[OLC_ZNUM(d)] = *OLC_ACTION(d);
   }

   create_command_list();
   sort_commands();

   add_to_save_list(AEDIT_PERMISSION, SL_ACTION);
   aedit_save_to_disk(d); /* autosave by Rumble */
}


/*------------------------------------------------------------------------*/

void aedit_save_to_disk(struct descriptor_data *d) {
   FILE *fp;
   int i;
   if (!(fp = fopen(SOCMESS_FILE_NEW, "w+")))  {
     char error[MAX_STRING_LENGTH];
     snprintf(error, sizeof(error), "Can't open socials file '%s'", SOCMESS_FILE);
     log("%s: %s", error, strerror(errno));
     exit(1);
   }

   for (i = 0; i <= top_of_socialt; i++)  {
      fprintf(fp, "~%s %s %d %d %d %d\n",
              soc_mess_list[i].command,
              soc_mess_list[i].sort_as,
              soc_mess_list[i].hide,
              soc_mess_list[i].min_char_position,
              soc_mess_list[i].min_victim_position,
              soc_mess_list[i].min_level_char);
      fprintf(fp, "%s\n%s\n%s\n%s\n",
              ((soc_mess_list[i].char_no_arg)?soc_mess_list[i].char_no_arg:"#"),
              ((soc_mess_list[i].others_no_arg)?soc_mess_list[i].others_no_arg:"#"),
              ((soc_mess_list[i].char_found)?soc_mess_list[i].char_found:"#"),
              ((soc_mess_list[i].others_found)?soc_mess_list[i].others_found:"#"));
      fprintf(fp, "%s\n%s\n%s\n%s\n",
              ((soc_mess_list[i].vict_found)?soc_mess_list[i].vict_found:"#"),
              ((soc_mess_list[i].not_found)?soc_mess_list[i].not_found:"#"),
              ((soc_mess_list[i].char_auto)?soc_mess_list[i].char_auto:"#"),
              ((soc_mess_list[i].others_auto)?soc_mess_list[i].others_auto:"#"));
      fprintf(fp, "%s\n%s\n%s\n",
              ((soc_mess_list[i].char_body_found)?soc_mess_list[i].char_body_found:"#"),
              ((soc_mess_list[i].others_body_found)?soc_mess_list[i].others_body_found:"#"),
              ((soc_mess_list[i].vict_body_found)?soc_mess_list[i].vict_body_found:"#"));
      fprintf(fp, "%s\n%s\n\n",
              ((soc_mess_list[i].char_obj_found)?soc_mess_list[i].char_obj_found:"#"),
              ((soc_mess_list[i].others_obj_found)?soc_mess_list[i].others_obj_found:"#"));
   }
   
   fprintf(fp, "$\n");
   fclose(fp);
   remove_from_save_list(AEDIT_PERMISSION, SL_ACTION); 
}

/*------------------------------------------------------------------------*/

/* Menu functions */

/* the main menu */
void aedit_disp_menu(struct descriptor_data * d) {
   struct social_messg *action = OLC_ACTION(d);

   write_to_output(d, 
           "@n-- Action editor\r\n"
           "@gn@n) Command         : @y%-15.15s@n @g1@n) Sort as Command  : @y%-15.15s@n\r\n"
           "@g2@n) Min Position[CH]: @c%-8.8s        @g3@n) Min Position [VT]: @c%-8.8s\r\n"
           "@g4@n) Min Level   [CH]: @c%-3d             @g5@n) Show if Invisible: @c%s\r\n"
           "@ga@n) Char    [NO ARG]: @c%s\r\n"
           "@gb@n) Others  [NO ARG]: @c%s\r\n"
           "@gc@n) Char [NOT FOUND]: @c%s\r\n"
           "@gd@n) Char  [ARG SELF]: @c%s\r\n"
           "@ge@n) Others[ARG SELF]: @c%s\r\n"
           "@gf@n) Char      [VICT]: @c%s\r\n"
           "@gg@n) Others    [VICT]: @c%s\r\n"
           "@gh@n) Victim    [VICT]: @c%s\r\n"
           "@gi@n) Char  [BODY PRT]: @c%s\r\n"
           "@gj@n) Others[BODY PRT]: @c%s\r\n"
           "@gk@n) Victim[BODY PRT]: @c%s\r\n"
           "@gl@n) Char       [OBJ]: @c%s\r\n"
           "@gm@n) Others     [OBJ]: @c%s\r\n"
           "@gq@n) Quit\r\n"
           "Enter Choice:",
           action->command,
           action->sort_as,
           position_types[action->min_char_position],
           position_types[action->min_victim_position],
           action->min_level_char,
           (action->hide?"HIDDEN":"NOT HIDDEN"),
           action->char_no_arg ? action->char_no_arg : "<Null>",
           action->others_no_arg ? action->others_no_arg : "<Null>",
           action->not_found ? action->not_found : "<Null>",
           action->char_auto ? action->char_auto : "<Null>",
           action->others_auto ? action->others_auto : "<Null>",
           action->char_found ? action->char_found : "<Null>",
           action->others_found ? action->others_found : "<Null>",
           action->vict_found ? action->vict_found : "<Null>",
           action->char_body_found ? action->char_body_found : "<Null>",
           action->others_body_found ? action->others_body_found : "<Null>",
           action->vict_body_found ? action->vict_body_found : "<Null>",
           action->char_obj_found ? action->char_obj_found : "<Null>",
           action->others_obj_found ? action->others_obj_found : "<Null>");

   OLC_MODE(d) = AEDIT_MAIN_MENU;
}


/*
 * The main loop
 */

void aedit_parse(struct descriptor_data * d, char *arg) {
   int i;

   switch (OLC_MODE(d)) {
    case AEDIT_CONFIRM_SAVESTRING:
      switch (*arg) {
       case 'y': case 'Y':
         aedit_save_internally(d);
         mudlog (CMP, ADMLVL_IMPL, TRUE, "OLC: %s edits action %s", 
                 GET_NAME(d->character), OLC_ACTION(d)->command);

         /* do not free the strings.. just the structure */
         cleanup_olc(d, CLEANUP_STRUCTS);
         write_to_output(d, "Action saved to disk.\r\n");
         break;
       case 'n': case 'N':
         /* free everything up, including strings etc */
         cleanup_olc(d, CLEANUP_ALL);
         break;
       default:
         write_to_output(d, "Invalid choice!\r\n"
                            "Do you wish to save this action internally? ");
         break;
      }
      return; /* end of AEDIT_CONFIRM_SAVESTRING */

    case AEDIT_CONFIRM_EDIT:
      switch (*arg)  {
       case 'y': case 'Y':
         aedit_setup_existing(d, OLC_ZNUM(d));
         break;
       case 'q': case 'Q':
         cleanup_olc(d, CLEANUP_ALL);
         break;
       case 'n': case 'N':
         OLC_ZNUM(d)++;
         for (;(OLC_ZNUM(d) <= top_of_socialt); OLC_ZNUM(d)++)
           if (is_abbrev(OLC_STORAGE(d), soc_mess_list[OLC_ZNUM(d)].command)) 
             break;

         if (OLC_ZNUM(d) > top_of_socialt) {
            if (aedit_find_command(OLC_STORAGE(d)) != -1)  {
               cleanup_olc(d, CLEANUP_ALL);
               break;
            }
            write_to_output(d, "Do you wish to add the '%s' action? ",
                               OLC_STORAGE(d));
            OLC_MODE(d) = AEDIT_CONFIRM_ADD;
         } else  {
            write_to_output(d, "Do you wish to edit the '%s' action? ",
                            soc_mess_list[OLC_ZNUM(d)].command);
            OLC_MODE(d) = AEDIT_CONFIRM_EDIT;
         }
         break;
       default:
         write_to_output(d, "Invalid choice!\r\n"
                            "Do you wish to edit the '%s' action? ", 
                            soc_mess_list[OLC_ZNUM(d)].command);
         break;
      }
      return;

    case AEDIT_CONFIRM_ADD:
      switch (*arg)  {
       case 'y': case 'Y':
         aedit_setup_new(d);
         break;
       case 'n': case 'N': case 'q': case 'Q':
         cleanup_olc(d, CLEANUP_ALL);
         break;
       default:
         write_to_output(d, "Invalid choice!\r\n"
                            "Do you wish to add the '%s' action? ", 
                            OLC_STORAGE(d));
         break;
      }
      return;

    case AEDIT_MAIN_MENU:
      switch (*arg) {
       case 'q': case 'Q':
         if (OLC_VAL(d))  { /* Something was modified */
            write_to_output(d, "Do you wish to save this action internally? ");
            OLC_MODE(d) = AEDIT_CONFIRM_SAVESTRING;
         }
         else cleanup_olc(d, CLEANUP_ALL);
         break;
       case 'n':
         write_to_output(d, "Enter action name: ");
         OLC_MODE(d) = AEDIT_ACTION_NAME;
         return;
       case '1':
         write_to_output(d, "Enter sort info for this action (for the command listing): ");
         OLC_MODE(d) = AEDIT_SORT_AS;
         return;
       case '2':
         write_to_output(d, "Enter the minimum position the Character has to be in to activate social:\r\n");
         {
           int i;
           for (i=POS_DEAD;i<=POS_STANDING;i++)
             write_to_output(d, "   %d) %s\r\n", i, position_types[i]);
           
           write_to_output(d, "Enter choice: ");
         }
         OLC_MODE(d) = AEDIT_MIN_CHAR_POS;
         return;
       case '3':
         write_to_output(d, "Enter the minimum position the Victim has to be in to activate social:\r\n");
         {
           int i;
           for (i=POS_DEAD;i<=POS_STANDING;i++)
             write_to_output(d, "   %d) %s\r\n", i, position_types[i]);
           
           write_to_output(d, "Enter choice: ");
         }
         OLC_MODE(d) = AEDIT_MIN_VICT_POS;
         return;
       case '4':
         write_to_output(d, "Enter new minimum level for social: ");
         OLC_MODE(d) = AEDIT_MIN_CHAR_LEVEL;
         return;
       case '5':
         OLC_ACTION(d)->hide = !OLC_ACTION(d)->hide;
         aedit_disp_menu(d);
         OLC_VAL(d) = 1;
         break;
       case 'a': case 'A':
         write_to_output(d, "Enter social shown to the Character when there is no argument supplied.\r\n"
                            "[OLD]: %s\r\n"
                            "[NEW]: ",
                 ((OLC_ACTION(d)->char_no_arg)?OLC_ACTION(d)->char_no_arg:"NULL"));
         OLC_MODE(d) = AEDIT_NOVICT_CHAR;
         return;
       case 'b': case 'B':
         write_to_output(d, "Enter social shown to Others when there is no argument supplied.\r\n"
                            "[OLD]: %s\r\n"
                            "[NEW]: ",
                            ((OLC_ACTION(d)->others_no_arg)?OLC_ACTION(d)->others_no_arg:"NULL"));
         OLC_MODE(d) = AEDIT_NOVICT_OTHERS;
         return;
       case 'c': case 'C':
         write_to_output(d, "Enter text shown to the Character when his victim isnt found.\r\n"
                            "[OLD]: %s\r\n"
                            "[NEW]: ",
                            ((OLC_ACTION(d)->not_found)?OLC_ACTION(d)->not_found:"NULL"));
         
         OLC_MODE(d) = AEDIT_VICT_NOT_FOUND;
         return;
       case 'd': case 'D':
         write_to_output(d, "Enter social shown to the Character when it is its own victim.\r\n"
                            "[OLD]: %s\r\n"
                            "[NEW]: ",
                            ((OLC_ACTION(d)->char_auto)?OLC_ACTION(d)->char_auto:"NULL"));
         
         OLC_MODE(d) = AEDIT_SELF_CHAR;
         return;
       case 'e': case 'E':
         write_to_output(d, "Enter social shown to Others when the Char is its own victim.\r\n"
                            "[OLD]: %s\r\n"
                            "[NEW]: ",
                            ((OLC_ACTION(d)->others_auto)?OLC_ACTION(d)->others_auto:"NULL"));
         
         OLC_MODE(d) = AEDIT_SELF_OTHERS;
         return;
       case 'f': case 'F':
         write_to_output(d, "Enter normal social shown to the Character when the victim is found.\r\n"
                            "[OLD]: %s\r\n"
                            "[NEW]: ",
                            ((OLC_ACTION(d)->char_found)?OLC_ACTION(d)->char_found:"NULL"));
         
         OLC_MODE(d) = AEDIT_VICT_CHAR_FOUND;
         return;
       case 'g': case 'G':
         write_to_output(d, "Enter normal social shown to Others when the victim is found.\r\n"
                            "[OLD]: %s\r\n"
                            "[NEW]: ",
                            ((OLC_ACTION(d)->others_found)?OLC_ACTION(d)->others_found:"NULL"));
         
         OLC_MODE(d) = AEDIT_VICT_OTHERS_FOUND;
         return;
       case 'h': case 'H':
         write_to_output(d, "Enter normal social shown to the Victim when the victim is found.\r\n"
                            "[OLD]: %s\r\n"
                            "[NEW]: ",
                            ((OLC_ACTION(d)->vict_found)?OLC_ACTION(d)->vict_found:"NULL"));
         
         OLC_MODE(d) = AEDIT_VICT_VICT_FOUND;
         return;
       case 'i': case 'I':
         write_to_output(d, "Enter 'body part' social shown to the Character when the victim is found.\r\n"
                            "[OLD]: %s\r\n"
                            "[NEW]: ",
                            ((OLC_ACTION(d)->char_body_found)?OLC_ACTION(d)->char_body_found:"NULL"));
         
         OLC_MODE(d) = AEDIT_VICT_CHAR_BODY_FOUND;
         return;
       case 'j': case 'J':
         write_to_output(d, "Enter 'body part' social shown to Others when the victim is found.\r\n"
                            "[OLD]: %s\r\n"
                            "[NEW]: ",
                            ((OLC_ACTION(d)->others_body_found)?OLC_ACTION(d)->others_body_found:"NULL"));
         
         OLC_MODE(d) = AEDIT_VICT_OTHERS_BODY_FOUND;
         return;
       case 'k': case 'K':
         write_to_output(d, "Enter 'body part' social shown to the Victim when the victim is found.\r\n"
                            "[OLD]: %s\r\n"
                            "[NEW]: ",
                            ((OLC_ACTION(d)->vict_body_found)?OLC_ACTION(d)->vict_body_found:"NULL"));
         
         OLC_MODE(d) = AEDIT_VICT_VICT_BODY_FOUND;
         return;
       case 'l': case 'L':
         write_to_output(d, "Enter 'object' social shown to the Character when the object is found.\r\n"
                            "[OLD]: %s\r\n"
                            "[NEW]: ",
                            ((OLC_ACTION(d)->char_obj_found)?OLC_ACTION(d)->char_obj_found:"NULL"));
         
         OLC_MODE(d) = AEDIT_OBJ_CHAR_FOUND;
         return;
       case 'm': case 'M':
         write_to_output(d, "Enter 'object' social shown to the Room when the object is found.\r\n"
                            "[OLD]: %s\r\n"
                            "[NEW]: ",
                            ((OLC_ACTION(d)->others_obj_found)?OLC_ACTION(d)->others_obj_found:"NULL"));
         
         OLC_MODE(d) = AEDIT_OBJ_OTHERS_FOUND;
         return;
       default:
         aedit_disp_menu(d);
         break;
      }
      return;
         
    case AEDIT_ACTION_NAME:
      if (!*arg || strchr(arg,' ')) {
        aedit_disp_menu(d);
        return;
      } 
      if (OLC_ACTION(d)->command)
        free(OLC_ACTION(d)->command);
        OLC_ACTION(d)->command = strdup(arg);

      break;

    case AEDIT_SORT_AS:
      if (!*arg || strchr(arg,' ')) {
        aedit_disp_menu(d);
        return;
      }
      if (OLC_ACTION(d)->sort_as) {
        free(OLC_ACTION(d)->sort_as);
        OLC_ACTION(d)->sort_as = strdup(arg);
      }
      break;

    case AEDIT_MIN_CHAR_POS:
    case AEDIT_MIN_VICT_POS:
      if (!*arg) {
        aedit_disp_menu(d);
        return;
      }
      i = atoi(arg);
      if ((i < POS_DEAD) && (i > POS_STANDING))  {
        aedit_disp_menu(d);
        return;
      } 
      if (OLC_MODE(d) == AEDIT_MIN_CHAR_POS)
        OLC_ACTION(d)->min_char_position = i;
      else
        OLC_ACTION(d)->min_victim_position = i;
      break;
      
    case AEDIT_MIN_CHAR_LEVEL:
      if (!*arg) {
        aedit_disp_menu(d);
        return;
      }
      i = atoi(arg);
      if (i < 0) {
        aedit_disp_menu(d);
        return;
      }
      OLC_ACTION(d)->min_level_char = i;
      break;

    case AEDIT_NOVICT_CHAR:
      if (OLC_ACTION(d)->char_no_arg)
        free(OLC_ACTION(d)->char_no_arg);
      if (*arg) {
        delete_doubledollar(arg);
        OLC_ACTION(d)->char_no_arg = strdup(arg);
      } else 
        OLC_ACTION(d)->char_no_arg = NULL;
      break;

    case AEDIT_NOVICT_OTHERS:
      if (OLC_ACTION(d)->others_no_arg)
        free(OLC_ACTION(d)->others_no_arg);
      if (*arg) {
        delete_doubledollar(arg);
        OLC_ACTION(d)->others_no_arg = strdup(arg);
      } else
        OLC_ACTION(d)->others_no_arg = NULL;
      break;

    case AEDIT_VICT_CHAR_FOUND:
      if (OLC_ACTION(d)->char_found)
        free(OLC_ACTION(d)->char_found);
      if (*arg) {
        delete_doubledollar(arg);
        OLC_ACTION(d)->char_found = strdup(arg);
      } else
        OLC_ACTION(d)->char_found = NULL;
      break;

    case AEDIT_VICT_OTHERS_FOUND:
      if (OLC_ACTION(d)->others_found)
        free(OLC_ACTION(d)->others_found);
      if (*arg) {
        delete_doubledollar(arg);
        OLC_ACTION(d)->others_found = strdup(arg);
      } else
        OLC_ACTION(d)->others_found = NULL;
      break;

    case AEDIT_VICT_VICT_FOUND:
      if (OLC_ACTION(d)->vict_found)
        free(OLC_ACTION(d)->vict_found);
      if (*arg) {
        delete_doubledollar(arg);
        OLC_ACTION(d)->vict_found = strdup(arg);
      } else
        OLC_ACTION(d)->vict_found = NULL;
      break;

    case AEDIT_VICT_NOT_FOUND:
      if (OLC_ACTION(d)->not_found)
        free(OLC_ACTION(d)->not_found);
      if (*arg) {
        delete_doubledollar(arg);
        OLC_ACTION(d)->not_found = strdup(arg);
      } else
        OLC_ACTION(d)->not_found = NULL;
      break;

    case AEDIT_SELF_CHAR:
      if (OLC_ACTION(d)->char_auto)
        free(OLC_ACTION(d)->char_auto);
      if (*arg) {
        delete_doubledollar(arg);
        OLC_ACTION(d)->char_auto = strdup(arg);
      } else
        OLC_ACTION(d)->char_auto = NULL;
      break;

    case AEDIT_SELF_OTHERS:
      if (OLC_ACTION(d)->others_auto)
        free(OLC_ACTION(d)->others_auto);
      if (*arg) {
        delete_doubledollar(arg);
        OLC_ACTION(d)->others_auto = strdup(arg);
      } else
        OLC_ACTION(d)->others_auto = NULL;
      break;

    case AEDIT_VICT_CHAR_BODY_FOUND:
      if (OLC_ACTION(d)->char_body_found)
        free(OLC_ACTION(d)->char_body_found);
      if (*arg) {
        delete_doubledollar(arg);
        OLC_ACTION(d)->char_body_found = strdup(arg);
      } else
        OLC_ACTION(d)->char_body_found = NULL;
      break;

    case AEDIT_VICT_OTHERS_BODY_FOUND:
      if (OLC_ACTION(d)->others_body_found)
        free(OLC_ACTION(d)->others_body_found);
      if (*arg) {
        delete_doubledollar(arg);
        OLC_ACTION(d)->others_body_found = strdup(arg);
      } else
        OLC_ACTION(d)->others_body_found = NULL;
      break;

    case AEDIT_VICT_VICT_BODY_FOUND:
      if (OLC_ACTION(d)->vict_body_found)
        free(OLC_ACTION(d)->vict_body_found);
      if (*arg) {
        delete_doubledollar(arg);
        OLC_ACTION(d)->vict_body_found = strdup(arg);
      } else
        OLC_ACTION(d)->vict_body_found = NULL;
      break;

    case AEDIT_OBJ_CHAR_FOUND:
      if (OLC_ACTION(d)->char_obj_found)
        free(OLC_ACTION(d)->char_obj_found);
      if (*arg) {
        delete_doubledollar(arg);
        OLC_ACTION(d)->char_obj_found = strdup(arg);
      } else
        OLC_ACTION(d)->char_obj_found = NULL;
      break;

    case AEDIT_OBJ_OTHERS_FOUND:
      if (OLC_ACTION(d)->others_obj_found)
        free(OLC_ACTION(d)->others_obj_found);
      if (*arg) {
        delete_doubledollar(arg);
        OLC_ACTION(d)->others_obj_found = strdup(arg);
      } else
        OLC_ACTION(d)->others_obj_found = NULL;
      break;

    default:
      /* we should never get here */
      break;
   }
   OLC_VAL(d) = 1;
   aedit_disp_menu(d);
}

ACMD(do_astat)
{
  int i, real = FALSE;
  char arg[MAX_INPUT_LENGTH];

  if (IS_NPC(ch))
    return;

  one_argument(argument, arg);

  if(!*arg) {
    send_to_char(ch, "Astat which social?\r\n");
    return;
  }

  for (i = 0; i <= top_of_socialt; i++) {
    if (is_abbrev(arg, soc_mess_list[i].command)) {
      real = TRUE;
      break;
    }
  }

  if (!real) {
    send_to_char(ch, "No such social.\r\n");
    return;
  }

   send_to_char(ch, 
    "n) Command         : @y%-15.15s@n 1) Sort as Command : @y%-15.15s@n\r\n"
    "2) Min Position[CH]: @c%-8.8s@n        3) Min Position[VT]: @c%-8.8s@n\r\n"
    "4) Min Level   [CH]: @c%-3d@n             5) Show if Invis   : @c%s@n\r\n"
    "a) Char    [NO ARG]: @c%s@n\r\n"
    "b) Others  [NO ARG]: @c%s@n\r\n"
    "c) Char [NOT FOUND]: @c%s@n\r\n"
    "d) Char  [ARG SELF]: @c%s@n\r\n"
    "e) Others[ARG SELF]: @c%s@n\r\n"
    "f) Char      [VICT]: @c%s@n\r\n"
    "g) Others    [VICT]: @c%s@n\r\n"
    "h) Victim    [VICT]: @c%s@n\r\n"
    "i) Char  [BODY PRT]: @c%s@n\r\n"
    "j) Others[BODY PRT]: @c%s@n\r\n"
    "k) Victim[BODY PRT]: @c%s@n\r\n"
    "l) Char       [OBJ]: @c%s@n\r\n"
    "m) Others     [OBJ]: @c%s@n\r\n",

    soc_mess_list[i].command,
    soc_mess_list[i].sort_as,
    position_types[soc_mess_list[i].min_char_position],
    position_types[soc_mess_list[i].min_victim_position],
    soc_mess_list[i].min_level_char,
    (soc_mess_list[i].hide ? "HIDDEN" : "NOT HIDDEN"),
    soc_mess_list[i].char_no_arg ? soc_mess_list[i].char_no_arg : "",
    soc_mess_list[i].others_no_arg ? soc_mess_list[i].others_no_arg : "",
    soc_mess_list[i].not_found ? soc_mess_list[i].not_found : "",
    soc_mess_list[i].char_auto ? soc_mess_list[i].char_auto : "",
    soc_mess_list[i].others_auto ? soc_mess_list[i].others_auto : "",
    soc_mess_list[i].char_found ? soc_mess_list[i].char_found : "",
    soc_mess_list[i].others_found ? soc_mess_list[i].others_found : "",
    soc_mess_list[i].vict_found ? soc_mess_list[i].vict_found : "",
    soc_mess_list[i].char_body_found ? soc_mess_list[i].char_body_found : "",
    soc_mess_list[i].others_body_found ? soc_mess_list[i].others_body_found : "",
    soc_mess_list[i].vict_body_found ? soc_mess_list[i].vict_body_found : "",
    soc_mess_list[i].char_obj_found ? soc_mess_list[i].char_obj_found : "",
    soc_mess_list[i].others_obj_found ? soc_mess_list[i].others_obj_found : "");

}

int aedit_find_command(const char *txt)
{
  int cmd;

  for (cmd = 1; *complete_cmd_info[cmd].command != '\n'; cmd++)
    if (!strncmp(complete_cmd_info[cmd].sort_as, txt, strlen(txt)) ||
        !strcmp(complete_cmd_info[cmd].command, txt))
      return (cmd);
  return (-1);
}

