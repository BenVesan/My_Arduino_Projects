/**********************************************************************************
Affichage de 2 bandes de Leds, et gestion de 3 boutons.



2024/04/16  V1.0  Création 
2024/04/18  V1.1  ajout de souplesse dans l'affichage des étapes dans le loop
2024/04/19  V1.2  Correction changements de mode et de couleurs, stockage en 
                  EEPROM de vitesse + couleurs + mode d'affichage et restitution 
                  des données au démarrage

**********************************************************************************/
#include <EEPROM.h>


  #define NOM_PROGRAMME "Panneau Lumineux SW"
  #define VERSION_PROGRAMME "v 1.2"
  #define DATE_PROGRAMME "2024/04/19"


//CONSTANTES
    #include <FastLED.h>
    #define CRGB_ELEMENT_SIZE 3
    #define DATA1_PIN 5
    #define DATA2_PIN 6


    #define LED_TYPE WS2812
    #define COLOR_ORDER GRB
    #define MAX_BRIGHTNESS  50
    #define COLOR_RED   CRGB(255,0,0)
    #define COLOR_GREEN CRGB(0,255,0)
    #define COLOR_BLUE  CRGB(0,0,255)
    #define COLOR_ORANGE CRGB(200,100,0)
    #define COLOR_WHITE CRGB(50,50,50)

    #define FACTEUR_ATTENUATION 5
    #define FACTEUR_ATTENUATION_TRAINEE 2
    #define NUM_LEDS 8
    #define ID_LEDS1 0
    #define ID_LEDS2 1


    #define PIN_BOUTON_BLANC 2
    #define PIN_BOUTON_ROUGE 3
    #define PIN_BOUTON_BLEU 4
    #define NOMBRE_BOUTONS 3

    #define DUREE_DEBOUNCE 20
    #define DUREE_APPUI_LONG 800
    #define DUREE_APPUI_TRES_LONG 2500

    #define APPUI_COURT_BOUTON_BLANC 1
    #define APPUI_COURT_BOUTON_BLEU 2
    #define APPUI_COURT_BOUTON_ROUGE 3
    #define APPUI_LONG_BOUTON_BLANC 4
    #define APPUI_LONG_BOUTON_BLEU 5
    #define APPUI_LONG_BOUTON_ROUGE 6
    #define APPUI_TRES_LONG_BOUTON_BLANC 7
    #define APPUI_TRES_LONG_BOUTON_BLEU 8
    #define APPUI_TRES_LONG_BOUTON_ROUGE 9
    #define RIEN_A_TRAITER 0

    #define MODE_POTENTIOMETRE 1
    #define MODE_SCANBAR 2
    #define MODE_GEAUGE_LINEAIRE 3
    #define MODE_GEAUGE_LINEAIRE_INVERSE 4
    #define MODE_OFF 5
    #define MODE_ALEATOIRE 6
    #define CHOIX_COULEUR_VERT_ORANGE_ROUGE 1
    #define CHOIX_COULEUR_BLANC 2
    #define CHOIX_COULEUR_BLEU 3
    #define CHOIX_COULEUR_ORANGE 4
    #define CHOIX_COULEUR_ROUGE 5

  #define FALSE 0
  #define TRUE 1


/******************************************************************************************************************************************************************
//VARIABLES
******************************************************************************************************************************************************************/



CRGB leds1[NUM_LEDS];
CRGB leds2[NUM_LEDS];
CRGB reference_leds1[NUM_LEDS];
CRGB reference_leds2[NUM_LEDS];
CRGB* tab_leds[2]={leds1,leds2};
CRGB* tab_ref_leds[2]={reference_leds1,reference_leds2};

unsigned int current_brightness=MAX_BRIGHTNESS;

// Variables Boutons
unsigned int pin_bouton[3]={PIN_BOUTON_BLANC,PIN_BOUTON_ROUGE,PIN_BOUTON_BLEU};
bool etat_bouton[NOMBRE_BOUTONS];
static volatile unsigned long TimerAppuiBouton[16];// Il faut autant d'éléments dans le tableau que de pins au total sur la carte Arduino !
static unsigned int Action_a_traiter=0;


// Variables modes et affichage
unsigned long timer_maintenant;
unsigned long timer_interruption;
unsigned long timer_dernier_affichage[2];
unsigned int vitesse_initiale_led[2]={500,300}; // Nombre de millisecondes entre deux etapes pour LED1, puis même chose pour LED2
unsigned int vitesse_led[2]={vitesse_initiale_led[ID_LEDS1],vitesse_initiale_led[ID_LEDS2]};
static int mode_affichage[2]={MODE_POTENTIOMETRE,MODE_GEAUGE_LINEAIRE};
static int etape[2]={500,0};
static int couleur[2];
static int direction=1;

// Variables EEPROM

// Vitesse, choix_couleur, Mode
struct Infos_Led {
  unsigned int vitesse;
  int couleur;
  int mode_affichage;
} ;
int taille_infos_Led=sizeof(struct Infos_Led);


/******************************************************************************************************************************************************************
******************************************************************************************************************************************************************
******************************************************************************************************************************************************************
Fonctions et procédures
******************************************************************************************************************************************************************
******************************************************************************************************************************************************************
******************************************************************************************************************************************************************/

/******************************************************************************************************************************************************************
Gestion des Leds
******************************************************************************************************************************************************************/

void init_tableaux_led()
/***********************************************************
Initialisation des tableaux actuels des Leds

***********************************************************/
{
  //FastLED.addLeds<LED_TYPE, DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.addLeds<LED_TYPE, DATA1_PIN,COLOR_ORDER>(leds1, NUM_LEDS);
  FastLED.addLeds<LED_TYPE, DATA2_PIN,COLOR_ORDER>(leds2, NUM_LEDS);
  FastLED.setBrightness(current_brightness);
  FastLED.clear();
}

void init_trois_couleurs_tableau(CRGB tableau_reference[],CRGB couleur1,unsigned int SeuilPourMille1,CRGB couleur2,unsigned int SeuilPourMille2,CRGB couleur3)
/***********************************************************
Initialisation du tableau de reference avec 3 couleurs

***********************************************************/
{
  unsigned int IdLed=0;
  while(IdLed < NUM_LEDS*SeuilPourMille1/1000)
  {
    tableau_reference[IdLed]=couleur1;
    IdLed++;
  }
  while(IdLed < NUM_LEDS*SeuilPourMille2/1000)
  {
    tableau_reference[IdLed]=couleur2;
    IdLed++;
  }
  while(IdLed < NUM_LEDS)
  {
    tableau_reference[IdLed]=couleur3;
    IdLed++;
  }
}
void assigne_tableau(CRGB tableau_assigne[], CRGB tableau_valeurs[],unsigned int nombre_elements_tableau = NUM_LEDS)
/***********************************************************
Deverse dans tableau_assigne le contenu de tableau_valeurs
***********************************************************/
{
    for (unsigned int idled = 0; idled < nombre_elements_tableau; idled++)
  {
    tableau_assigne[idled]=tableau_valeurs[idled];
  }
}

void defini_luminosite_tableau(CRGB tableau[], CRGB tableau_reference[], unsigned int luminosite)
/***********************************************************
Modifie la luminosite de toutes les leds du tableau 
par rapport à la luminosité du tableau  de référence
***********************************************************/
{
    for (unsigned int idled = 0; idled < NUM_LEDS; idled++)
    {
      tableau[idled].red=tableau_reference[idled].red*luminosite/current_brightness;
      tableau[idled].green=tableau_reference[idled].green*luminosite/current_brightness;
      tableau[idled].blue=tableau_reference[idled].blue*luminosite/current_brightness;
    }
}

void allumage_tableau(CRGB tableau_assigne[], CRGB tableau_reference[], unsigned int nombre_flash = 3, unsigned int delai=100)
/***********************************************************
Allumage du tableau progressivement, précédé de flashes
***********************************************************/
{
  unsigned int luminosite = 0;
  //assigne_tableau(tableau_assigne,leds_reference,NUM_LEDS);
  assigne_tableau(tableau_assigne,tableau_reference,NUM_LEDS);
   /*Exctinction totale*/
  //FastLED.setBrightness(0);
  defini_luminosite_tableau(tableau_assigne,tableau_reference,0);
  FastLED.show();
  delay(100);
  /*boucle flash*/
  for (unsigned int cpt = 0; cpt < nombre_flash; cpt++)
  {
    luminosite=current_brightness * (random(100))/200;
    //FastLED.setBrightness(luminosite);
    defini_luminosite_tableau(tableau_assigne,tableau_reference,luminosite);
    FastLED.show();
    delay(100);
    luminosite=current_brightness * (rand());
    //FastLED.setBrightness(luminosite);
    defini_luminosite_tableau(tableau_assigne,tableau_reference,luminosite);
    FastLED.show();
    delay(100);
    //FastLED.setBrightness(0);
    defini_luminosite_tableau(tableau_assigne,tableau_reference,0);
    FastLED.show();
    delay(delai);
    delay(random(500));
  }
  /* Boucle allumage progressif*/
  for (int i = 1; sq(i)< current_brightness;i++)  
  {
    //FastLED.setBrightness(i*i);
    defini_luminosite_tableau(tableau_assigne,tableau_reference,i*i);
  FastLED.show();
  delay(delai);
  }
}

void extinction_tableau(CRGB tableau_assigne[],CRGB tableau_reference[], unsigned int delai=200)
/***********************************************************
Exctinction progressive du tableau
***********************************************************/
{
  assigne_tableau(tableau_assigne,tableau_reference);
  for (int i = sqrt(current_brightness); i> 0;i--)  
    {
      //FastLED.setBrightness(i*i);
      defini_luminosite_tableau(tableau_assigne,tableau_reference,i*i);
      FastLED.show();
      delay(delai);
    }
}


void test_tableau_led(CRGB tableau_assigne[], int delai = 100)
/***********************************************************
Affichage de toutes les leds du tableau, une apr une, 
en cyclant sur les couleurs (Rouge puis Vert puis Bleu)

***********************************************************/
{
   for (int i = 0;i< NUM_LEDS;i++)
  {
		tableau_assigne[i]=COLOR_RED;
    FastLED.show();
    delay(delai);
    
    tableau_assigne[i]=COLOR_GREEN;
    FastLED.show();
    delay(delai);

    tableau_assigne[i]=COLOR_BLUE;
    FastLED.show();
    delay(delai);
	}
  

}

void affiche_tableau_led_pourmilles(CRGB tableau_assigne[], CRGB tableau_reference[], unsigned int PourMille)
/***********************************************************
Allume le tableau des Leds en fonction du "pourmille" 
0 : Aucune Led allumée
1000 : toutes les Leds allumées
***********************************************************/
{
  unsigned int IdLed=0;
  unsigned int MaxLed;
  unsigned int ProportionLuminosite;
  
  PourMille=min(PourMille,1000);
  
// Calcul des valeurs des seuils
// Nombre de Leds allumées complètement
   MaxLed=int(PourMille*NUM_LEDS/1000);
// Proportion de luminosité pour la dernière Led
// On utilise ici un carré (fonction sq) pour que l'effet visuel soit linéaire.
   ProportionLuminosite=sq(((PourMille % (1000/NUM_LEDS))*NUM_LEDS/10))/100;
  while(IdLed < MaxLed)// Int divisé par Int doit donner int, c'est donc censé être une division euclidienne
  {
    tableau_assigne[IdLed]=tableau_reference[IdLed];
    IdLed++;
  }
 if( ( IdLed < NUM_LEDS)  )
  {
  //Allumage a un pourcentage de current_brightness de la Led suivante
    tableau_assigne[IdLed].red = int(tableau_reference[IdLed].red*ProportionLuminosite/100);
    tableau_assigne[IdLed].green = int(tableau_reference[IdLed].green*ProportionLuminosite/100);
    tableau_assigne[IdLed].blue = int(tableau_reference[IdLed].blue*ProportionLuminosite/100);
  if ( ProportionLuminosite > 0)  IdLed++;
  }
   // Extinction de toutes les autres Leds
  while(IdLed < NUM_LEDS )
  {
    tableau_assigne[IdLed].red=0;
    tableau_assigne[IdLed].green=0;
    tableau_assigne[IdLed].blue=0;
    IdLed++;
  }
 FastLED.show();
}


void affiche_cycle_tableau_etape(CRGB tableau_assigne[], CRGB couleur_reference[], unsigned int led_etape=0, int direction = 1, unsigned int nombre_leds_trainee=1)
/***********************************************************
Allume une Led (principale) du tableau, 
ainsi que sa ou ses leds de trainée, avec une luminosité atténuée
en fonction de la direction du "déplacement" dela Led principale
***********************************************************/
{
  unsigned int attenuation_trainee;
  unsigned int led_trainee;
  // Reinitialisation des Leds du tableau
  for (int IdLed=0;IdLed < NUM_LEDS;IdLed++)
  {
    tableau_assigne[IdLed].red=0;
    tableau_assigne[IdLed].green=0;
    tableau_assigne[IdLed].blue=0;
  }
  if ( direction == 1 )
  {
  // Affichage de la Led étape, et de la trainée, leds inférieures
    tableau_assigne[led_etape]=couleur_reference[led_etape];
    attenuation_trainee = FACTEUR_ATTENUATION_TRAINEE * FACTEUR_ATTENUATION_TRAINEE;
    for (unsigned int cpt_trainee=1;cpt_trainee <=nombre_leds_trainee;cpt_trainee++)
    {
            led_trainee= led_etape - cpt_trainee;
            if (led_trainee >=0 and led_trainee <NUM_LEDS)
            {
              attenuation_trainee = attenuation_trainee * FACTEUR_ATTENUATION_TRAINEE ;
              tableau_assigne[led_trainee].red = int(couleur_reference[led_trainee].red/attenuation_trainee);
              tableau_assigne[led_trainee].green = int(couleur_reference[led_trainee].green/attenuation_trainee);
              tableau_assigne[led_trainee].blue = int(couleur_reference[led_trainee].blue/attenuation_trainee);
            }
    }
  }
  else
  {
    // Affichage de la Led étape, et de la trainée, leds supérieures
     tableau_assigne[led_etape]=couleur_reference[led_etape];
      
    attenuation_trainee = FACTEUR_ATTENUATION_TRAINEE * FACTEUR_ATTENUATION_TRAINEE;
    for (unsigned int cpt_trainee=1; cpt_trainee <=nombre_leds_trainee;cpt_trainee++)
    {
            led_trainee= led_etape + cpt_trainee;
            if (led_trainee >=0 and led_trainee <NUM_LEDS)
            {
              attenuation_trainee = attenuation_trainee * FACTEUR_ATTENUATION_TRAINEE ;
                  tableau_assigne[led_trainee].red = int(couleur_reference[led_trainee].red/attenuation_trainee);
                  tableau_assigne[led_trainee].green = int(couleur_reference[led_trainee].green/attenuation_trainee);
                  tableau_assigne[led_trainee].blue = int(couleur_reference[led_trainee].blue/attenuation_trainee);
            }
    }
  }
 FastLED.show();
}




void affiche_leds_aleatoire(CRGB tableau_assigne[], CRGB couleur_reference[],unsigned int nombre_leds=2)
/***********************************************************
Allume <nombre_leds> Leds du tableau, 
choisies de manière aléatoires
***********************************************************/
{
  unsigned int tab_temp[NUM_LEDS];
  unsigned int id_rand;
  // Reinitialisation des Leds du tableau
  for (int IdLed=0;IdLed < NUM_LEDS;IdLed++)
  {
    tableau_assigne[IdLed].red=0;
    tableau_assigne[IdLed].green=0;
    tableau_assigne[IdLed].blue=0;
    tab_temp[IdLed]=0;
  }
  // Init valeurs aleatoires
  nombre_leds=min(nombre_leds,NUM_LEDS-1);
  for (unsigned int cpt=0;cpt<nombre_leds;cpt++)
  {
    // Generation d'un Id de Led à allumer aléatoire, et vérification que cette Led n'est pas déjà allumée
    id_rand=random(0,NUM_LEDS-1);
    while (tab_temp[id_rand] !=0)
    {
      id_rand=random(0,NUM_LEDS-1);
    }
    // A ce stade on a trouvé une Led non allumée 
    tab_temp[id_rand]=1;
    tableau_assigne[id_rand]=couleur_reference[id_rand];

  }
  FastLED.show();
}

/******************************************************************************************************************************************************************
Gestion des boutons


******************************************************************************************************************************************************************/



void changement_etat_bouton(unsigned int pin_Bouton,bool etat_bouton)
/***********************************************************
Déclenche un Timer à l'appui d'un bouton, 
et Valorise "Action_a_traiter" lors de la relache d'un bouton
en fonction de la durée d'appui
***********************************************************/
{
  if ( etat_bouton == LOW)
  {
    //Serial.println("Appui Bouton ");Serial.println(pin_Bouton);
    // Bouton pressé
    TimerAppuiBouton[pin_Bouton] = millis();    
  }
  else // Bouton relaché
  {
    timer_interruption=millis();

    if ( timer_interruption - TimerAppuiBouton[pin_Bouton] < DUREE_DEBOUNCE  )
  {
    return; // Si la duree d'appui est trop courte, on sort sans rien faire
  }
  if ( timer_interruption - TimerAppuiBouton[pin_Bouton] > DUREE_APPUI_TRES_LONG ) // Si la duree dépasse le seuil d'appui très long, on annonce un appui très long
  {
    //Serial.println("relache bouton Tres Long :");Serial.println(pin_Bouton);
    //Serial.println("Maintenant");Serial.println(timer_interruption);
    //Serial.println("Appui ");Serial.println(TimerAppuiBouton[pin_Bouton]);
    
    switch (pin_Bouton)
    {
      case PIN_BOUTON_BLANC : Action_a_traiter=APPUI_TRES_LONG_BOUTON_BLANC;break;
      case PIN_BOUTON_BLEU : Action_a_traiter=APPUI_TRES_LONG_BOUTON_BLEU;break;
      case PIN_BOUTON_ROUGE : Action_a_traiter=APPUI_TRES_LONG_BOUTON_ROUGE;break;
      default :  Serial.println("relache bouton Tres Long bouton inconnu");
    }
    
  }
  else if ( timer_interruption - TimerAppuiBouton[pin_Bouton] > DUREE_APPUI_LONG ) // Si la duree dépasse le seuil d'appui long, on annonce un appui long
  {
    //Serial.println("relache bouton Long  : ");Serial.println(pin_Bouton);
    //Serial.println("Maintenant");Serial.println(timer_interruption);
    //Serial.println("Appui ");Serial.println(TimerAppuiBouton[pin_Bouton]);
    
    switch (pin_Bouton)
    {
      case PIN_BOUTON_BLANC : Action_a_traiter=APPUI_LONG_BOUTON_BLANC;break;
      case PIN_BOUTON_BLEU : Action_a_traiter=APPUI_LONG_BOUTON_BLEU;break;
      case PIN_BOUTON_ROUGE : Action_a_traiter=APPUI_LONG_BOUTON_ROUGE;break;
      default :  Serial.println("relache bouton Long bouton inconnu");
    }
    
  }
  else                                                // Si la duree ne dépasse pas le seuil d'appui long, on annonce un appui court
  {
    //Serial.println("relache bouton Court :");Serial.println(pin_Bouton);
    //Serial.println("Maintenant");Serial.println(timer_interruption);
    //Serial.println("Appui ");Serial.println(TimerAppuiBouton[pin_Bouton]);
    
    switch (pin_Bouton)
    {
      case PIN_BOUTON_BLANC : Action_a_traiter=APPUI_COURT_BOUTON_BLANC;break;
      case PIN_BOUTON_BLEU : Action_a_traiter=APPUI_COURT_BOUTON_BLEU;break;
      case PIN_BOUTON_ROUGE : Action_a_traiter=APPUI_COURT_BOUTON_ROUGE;break;
      default :  Serial.println("relache bouton Court bouton inconnu");
    }
    
  }
  }
}

void change_vitesse(unsigned int id_led, int vitesse=-1)
/***********************************************************
Change la "vitesse" d'affichage d'un tableau, 
en valorisant vitesse_led[id_led]
***********************************************************/
{
  if (vitesse == -1)
  {
    vitesse_led[id_led]=vitesse_led[id_led]*2;
  }
  else
  {
    vitesse_led[id_led]=vitesse;
  }

    if (vitesse_led[id_led] > vitesse_initiale_led[id_led] * 32) vitesse_led[id_led] = vitesse_initiale_led[id_led];
    if (vitesse_led[id_led] < vitesse_initiale_led[id_led]) vitesse_led[id_led] = vitesse_initiale_led[id_led];
    // Mise à jour des informations dans l'EEPROM
    ecriture_infos_Led(id_led);
}


void change_couleur(unsigned int id_led,int choix_couleur =-1)
/***********************************************************
Modifie le tableau de référence de couleurs 
pour un tableau de leds.
***********************************************************/
{
  //Serial.println("Changement couleurs");
  //Serial.println(id_led);
  //FastLED.clear();
  int ancien_choix_couleur=couleur[id_led];
  if (choix_couleur == -1)
  {
    switch(ancien_choix_couleur)
    {
      case CHOIX_COULEUR_ROUGE :couleur[id_led]=CHOIX_COULEUR_ORANGE;break;
      case CHOIX_COULEUR_ORANGE :couleur[id_led]=CHOIX_COULEUR_BLANC;break;
      case CHOIX_COULEUR_BLANC :couleur[id_led]=CHOIX_COULEUR_BLEU;break;
      case CHOIX_COULEUR_BLEU :couleur[id_led]=CHOIX_COULEUR_VERT_ORANGE_ROUGE;break;
      case CHOIX_COULEUR_VERT_ORANGE_ROUGE :couleur[id_led]=CHOIX_COULEUR_ROUGE;break;
      default : couleur[id_led]=CHOIX_COULEUR_ROUGE;break;
    }

  }
  else
  {
    couleur[id_led]=choix_couleur;
  }
  
  switch (couleur[id_led])
  {
    case CHOIX_COULEUR_ROUGE :  //Serial.println("COULEUR ROUGE");
                                init_trois_couleurs_tableau(tab_ref_leds[id_led], COLOR_RED,625,COLOR_RED,875,COLOR_RED);
                                break;
    case CHOIX_COULEUR_ORANGE :  //Serial.println("COULEUR ORANGE");
                                init_trois_couleurs_tableau(tab_ref_leds[id_led], COLOR_ORANGE,625,COLOR_ORANGE,875,COLOR_ORANGE);
                                break;
    case CHOIX_COULEUR_BLANC :  //Serial.println("COULEUR BLANC");
                                init_trois_couleurs_tableau(tab_ref_leds[id_led], COLOR_WHITE,625,COLOR_WHITE,875,COLOR_WHITE);
                                break;
    case CHOIX_COULEUR_BLEU :   //Serial.println("COULEUR BLEUE");
                                init_trois_couleurs_tableau(tab_ref_leds[id_led], COLOR_BLUE,625,COLOR_BLUE,875,COLOR_BLUE);
                                break;
    case CHOIX_COULEUR_VERT_ORANGE_ROUGE ://Serial.println("COULEUR VERT/ORAGNE/ROUGE");
                                          init_trois_couleurs_tableau(tab_ref_leds[id_led], COLOR_GREEN,625,COLOR_ORANGE,875,COLOR_RED);  
                                          break;
  }
  // Mise à jour des informations dans l'EEPROM
  ecriture_infos_Led(id_led);
}


void change_mode(unsigned int id_led,int mode=-1)
/***********************************************************
Modifie le mode d'affichage d'un tableau de leds.
Soit en spécifiant le nouveau mode, soit en laissant 
le passage vers le prochain mode automatique
***********************************************************/
{

  int ancien_mode=mode_affichage[id_led];
  if (mode == -1)
  {
    switch (ancien_mode)
    {
      case MODE_OFF : mode_affichage[id_led]=MODE_POTENTIOMETRE;break;
      case MODE_POTENTIOMETRE : mode_affichage[id_led]=MODE_SCANBAR;break;
      case MODE_SCANBAR : mode_affichage[id_led]=MODE_GEAUGE_LINEAIRE;break;
      case MODE_GEAUGE_LINEAIRE : mode_affichage[id_led]=MODE_GEAUGE_LINEAIRE_INVERSE;break;
      case MODE_GEAUGE_LINEAIRE_INVERSE : mode_affichage[id_led]=MODE_ALEATOIRE;break;
      case MODE_ALEATOIRE : mode_affichage[id_led]=MODE_OFF;break;
      default : mode_affichage[id_led]=MODE_ALEATOIRE;break;
    }
  }
  else
  {
    mode_affichage[id_led]=mode;
  }
  
  //Serial.println("Changement Mode");
  //Serial.println(id_led);
  switch(mode_affichage[id_led])
  {
    case MODE_SCANBAR : Serial.println("MODE SCANBAR");
                        etape[id_led]=0;
                        direction=1;
                        break;

    case MODE_GEAUGE_LINEAIRE : Serial.println("MODE GEAUGE LINEAIRE");
                                etape[id_led]=0;
                                break;

    case MODE_GEAUGE_LINEAIRE_INVERSE : Serial.println("MODE GEAUGE LINEAIRE INVERSEE");
                                        etape[id_led]=1000;
                                        break;

    case MODE_OFF :           Serial.println("MODE OFF");
                              etape[id_led]=0;
                              for(int cpt = 0;cpt <NUM_LEDS;cpt++) 
                              {
                                tab_leds[id_led][cpt]=CRGB(0,0,0);
                              }
                              FastLED.show();
                              break;    

    case MODE_ALEATOIRE :     Serial.println("MODE_ALEATOIRE");
                              etape[id_led]=500;
                              break;

    case MODE_POTENTIOMETRE : Serial.println("MODE OPTENTIOMETRE");
                              etape[id_led]=500;
                              break;
  }
  // Mise à jour des informations dans l'EEPROM
  ecriture_infos_Led(id_led);  
}


void affiche(unsigned int id_leds)
/***********************************************************
Affiche un tableau de Leds en fonction 
du mode d'affichage en cours
***********************************************************/
{
  switch(mode_affichage[id_leds])
  {
    case MODE_POTENTIOMETRE :   affiche_tableau_led_pourmilles(tab_leds[id_leds],tab_ref_leds[id_leds],etape[id_leds]); 
                                if  ( etape[id_leds] > 900 ) etape[id_leds]=min(999,max(65,etape[id_leds]+random(-200,0))); // on facilite la baisse si on est déjà dans le rouge
                                if  ( etape[id_leds] < 400 ) etape[id_leds]=min(999,max(65,etape[id_leds]+random(0,200)));  // on facilite la hausse si on est trop dans le vert
                                etape[id_leds]=min(999,max(65,etape[id_leds]+random(-100,100)));
                                break;
    case MODE_SCANBAR :         affiche_cycle_tableau_etape(tab_leds[id_leds],tab_ref_leds[id_leds],etape[id_leds],direction);
                                etape[id_leds]=etape[id_leds]+direction;
                                if (etape[id_leds] > 7)
                                {
                                  //Serial.println("ScanBar : sens rebourd");
                                  direction=-1;
                                  etape[id_leds]=7;
                                }
                                if (etape[id_leds] < 0)
                                {
                                  //Serial.println("ScanBar : sens normal");
                                  direction=1;
                                  etape[id_leds]=0;
                                }
                                break;
    case MODE_GEAUGE_LINEAIRE:  affiche_tableau_led_pourmilles(tab_leds[id_leds],tab_ref_leds[id_leds],etape[id_leds]);
                                etape[id_leds]=etape[id_leds]+125;
                                if (etape[id_leds] > 1000 ) etape[id_leds]=0;
                                break;
    case MODE_GEAUGE_LINEAIRE_INVERSE :                            
                                affiche_tableau_led_pourmilles(tab_leds[id_leds],tab_ref_leds[id_leds],etape[id_leds]);
                                etape[id_leds]=etape[id_leds]-125;
                                if (etape[id_leds] <= 0 ) etape[id_leds]=1000;
                                break;
    case MODE_ALEATOIRE               :
                                //affiche_leds_aleatoire(tab_leds[id_leds],tab_ref_leds[id_leds],2);
                                affiche_leds_aleatoire(tab_leds[id_leds],tab_ref_leds[id_leds],random(1,4));
                                break;

  }
  
}


/******************************************************************************************************************************************************************
Gestion EEPROM
******************************************************************************************************************************************************************/

void chargement_infos_Led(unsigned int id_led)
{
  int adresse=taille_infos_Led*id_led;
  Infos_Led temp_info_led;
  EEPROM.get(adresse,temp_info_led);
  //vitesse_led[id_led]=temp_info_led.vitesse;
  //couleur[id_led]=temp_info_led.couleur;
  //mode_affichage[id_led]=temp_info_led.mode_affichage;
  Serial.println("****Chargement infos****");
  Serial.println("OFFSet");
  Serial.println(adresse);
  Serial.println("Id Leds");
  Serial.println(id_led);
  Serial.println("Vitesse");
  Serial.println(temp_info_led.vitesse);
  Serial.println("couleur");
  Serial.println(temp_info_led.couleur);
  Serial.println("Mode");
  Serial.println(temp_info_led.mode_affichage);
  change_vitesse(id_led,temp_info_led.vitesse);
  change_couleur(id_led,temp_info_led.couleur);
  change_mode(id_led,temp_info_led.mode_affichage);
  
}

void ecriture_infos_Led(unsigned int id_led)
{
  /***********************************************************
  Lit dans l'EEPROM les données relatives au tableau id_led 
 et met à jour les variables relatives à ce tableau
 (vitesse, choix des couleurs, mode d'affichage)
***********************************************************/
  int adresse=taille_infos_Led*id_led;
  Infos_Led temp_info_led;
  temp_info_led.vitesse=vitesse_led[id_led];
  temp_info_led.couleur=couleur[id_led];
  temp_info_led.mode_affichage=mode_affichage[id_led];
  EEPROM.put(adresse,temp_info_led);
  Serial.println("****Ecriture infos****");
  Serial.println("OFFSet");
  Serial.println(adresse);
  Serial.println("Id Leds");
  Serial.println(id_led);
  Serial.println("Vitesse");
  Serial.println(temp_info_led.vitesse);
  Serial.println("couleur");
  Serial.println(temp_info_led.couleur);
  Serial.println("Mode");
  Serial.println(temp_info_led.mode_affichage);
}





/******************************************************************************************************************************************************************
******************************************************************************************************************************************************************
******************************************************************************************************************************************************************
                                          Programme principal
******************************************************************************************************************************************************************
******************************************************************************************************************************************************************
******************************************************************************************************************************************************************/


void setup() {
  Serial.begin(9600);
  Serial.println(NOM_PROGRAMME);
  Serial.println(VERSION_PROGRAMME);
  Serial.println(DATE_PROGRAMME);


    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(PIN_BOUTON_BLANC, INPUT_PULLUP);
    pinMode(PIN_BOUTON_ROUGE, INPUT_PULLUP);
    pinMode(PIN_BOUTON_BLEU, INPUT_PULLUP);

  // Clignotement de la Led interne, pour montrer que l'on vient de débuter l'exécution du programme
  for(int i = 0; i< 5; i++)  
  {
        digitalWrite(LED_BUILTIN, HIGH);  
        delay(300);                      
        digitalWrite(LED_BUILTIN, LOW);  
        delay(100);                      
  }


  init_tableaux_led();
/*
  change_couleur(ID_LEDS1,CHOIX_COULEUR_VERT_ORANGE_ROUGE);
  change_couleur(ID_LEDS2,CHOIX_COULEUR_BLANC);
  change_mode(ID_LEDS1,MODE_POTENTIOMETRE);
  change_mode(ID_LEDS2,MODE_ALEATOIRE);
*/
  chargement_infos_Led(ID_LEDS1);
  chargement_infos_Led(ID_LEDS2);

  /*
  allumage_tableau(leds1,reference_leds1);
  allumage_tableau(leds2, reference_leds2);
  extinction_tableau(leds1,reference_leds1);
  extinction_tableau(leds2,reference_leds2);
  */

  for(unsigned int cpt=0;cpt<NOMBRE_BOUTONS;cpt++) etat_bouton[cpt]=HIGH;
  timer_dernier_affichage[ID_LEDS1]=millis();
  timer_dernier_affichage[ID_LEDS2]=millis();


}

void loop() 
{

  ////////////////////////////////////////// Gestion des appuis/relaches de boutons
  for(unsigned int cpt=0;cpt<NOMBRE_BOUTONS;cpt++)
  {
      if (etat_bouton[cpt] != digitalRead(pin_bouton[cpt]) )// Un bouton vient d'être appuyé ou relâché
      {
          etat_bouton[cpt]=!etat_bouton[cpt];
          changement_etat_bouton(pin_bouton[cpt],etat_bouton[cpt]);
      }
  }
 
 ////////////////////////////////////////// Gestion des actions à traiter
  switch (Action_a_traiter)
  {
    case APPUI_TRES_LONG_BOUTON_BLANC :  //Serial.println("APPUI TRES LONG BOUTON BLANC");
                                    change_vitesse(ID_LEDS1);
                                    Action_a_traiter=RIEN_A_TRAITER;
                                    break;
    case APPUI_TRES_LONG_BOUTON_ROUGE :  //Serial.println("APPUI TRES LONG BOUTON ROUGE");
                                    change_vitesse(ID_LEDS2);
                                    Action_a_traiter=RIEN_A_TRAITER;
                                    break;
    case APPUI_LONG_BOUTON_BLANC :  //Serial.println("APPUI LONG BOUTON BLANC");
                                    change_mode(ID_LEDS1);
                                    Action_a_traiter=RIEN_A_TRAITER;
                                    break;
    case APPUI_LONG_BOUTON_ROUGE :  //Serial.println("APPUI LONG BOUTON ROUGE");
                                    change_mode(ID_LEDS2);
                                    Action_a_traiter=RIEN_A_TRAITER;
                                    break;                                
    case APPUI_COURT_BOUTON_BLANC : //Serial.println("APPUI COURT BOUTON BLANC");
                                    change_couleur(ID_LEDS1);
                                    Action_a_traiter=RIEN_A_TRAITER;
                                    break;
    case APPUI_COURT_BOUTON_ROUGE : //Serial.println("APPUI LONG BOUTON ROUGE");
                                    change_couleur(ID_LEDS2);
                                    Action_a_traiter=RIEN_A_TRAITER;
                                    break;
  }

////////////////////////////////////////// Gestion des affichages
timer_maintenant=millis();

if (( timer_maintenant > timer_dernier_affichage[ID_LEDS1] +vitesse_led[ID_LEDS1] ) )
{
	affiche(ID_LEDS1);
  timer_dernier_affichage[ID_LEDS1]=millis();
}
if (( timer_maintenant > timer_dernier_affichage[ID_LEDS2] +vitesse_led[ID_LEDS2] ))
{
	affiche(ID_LEDS2);
  timer_dernier_affichage[ID_LEDS2]=millis();
}

}
