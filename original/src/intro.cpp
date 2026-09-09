//--------------------------------------------------------------------------//
// iq / rgba  .  tiny codes  .  2008                                        //
//--------------------------------------------------------------------------//

#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <math.h>
#include "config.h"
#include "ext.h"
#include "mzk.h"
#include "minimath.h"
//=================================================================================================================

#define OBJ_PLANE 0
#define OBJ_SPHERE 1
#define OBJ_CUERNO 2
#define OBJ_LOGO 3
#define ALTERNATE_LOADING
//#define GAMMA
//#define NO_SHADE
//#define LO_RES

// Structs

typedef struct
{
	float vcol[4];
	float vnor[3];
	float vpos[3];
//	int	  hdr_color;
} str_vertex;
typedef struct  
{
	str_vertex verts[50000];
	int quads[150000];
	int nquads,nverts;
	int text_ini,text_num;
	
} str_scene;
typedef struct
{
	char dastr[8];
	char num;
	float vpos[2];
} str_text;
#define pantsize 64
#define SCENES 7

HFONT		font;
HANDLE		thH;
void		*writebuf;
int			tID;
int			size;

str_scene sc_normal[SCENES];
str_scene sc_simple[SCENES];
str_scene *pscene;

unsigned char  ipantalla[pantsize*pantsize];
//float  fpantalla[pantsize*pantsize*4];

str_text interpol, *ptexts;
str_vertex *pvert;
int demostate;
int	icnt1,icnt2,icnt3,iacc1;
int *pntexts;
int *pquad,*pnquads,*pnverts;//,base_vert;
int cnt_u,cnt_v,cnt_comp,cnt_vrt,ktrx,ktry,cnt_scn;
int res_u,res_v,res_ulow,res_vlow,res_obj;
float facc1, facc2;
float cal_u,cal_v,ptmpvert[8],ptmppos[9],f_calc;
float damatrix[16];

short fourierx1s[34] = {32767, -5596, 6098, -6022, 10259, 2860, -1323, 1504, 1723, -1815, -766, -45, -219, 
						822, 62, -304, 186, -147, 140, 173, -317, -117, -29, 22, 291, 117, -108, -98,
					   -124, -58, 117, 72, -26, 39};

str_text scene_texts[7]={"fuzzion",		7,	-2,		0,
						 ".chucho",		7,	-2,		1.4f,
						 "bp",			2,	0.2f,	1.4f,
						 "Ufix",		4,	-5.f,	-.6f,
						 "Pain",		4,	2.0f,	-.6f,	
						 "Wonder",		6,	-1.2f,	-2.2f,
						 "Loading",		7,	-1.f,	-2.5f
};

int text_numbers[SCENES]={1,0,0,1,0,4,0};
float PI =			  3.1415926535897932384626433832795f;
float cnt105 =		  1.05946309436f;
float cnt33k =		  1378.125;
GLbyte c_black[4]={0,0,0,0};

//=================================================================================================================


//=================================================================================================================

void draw_scena (void)
{
	glColor4bv(c_black);
	glDrawElements(GL_QUADS,pscene->nquads,GL_UNSIGNED_INT,pscene->quads);
	glColor4bv(c_black);
	ptexts=scene_texts+pscene->text_ini;
#ifdef NO_SHADE
	glDisable(GL_LIGHTING);
#endif
	for (cnt_comp=pscene->text_ini;cnt_comp<pscene->text_num;cnt_comp++)
	{
		glPushMatrix();
//		glTranslatef(ptexts->vpos[0],ptexts->vpos[1],ptexts->vpos[2]);
		glTranslatef(ptexts->vpos[0],ptexts->vpos[1],-1);
		glScalef(2,2,2);
//		glRotatef(ptexts->rot,0,1,0);
		glCallLists(ptexts->num,GL_UNSIGNED_BYTE,ptexts->dastr);
		ptexts++;
		glPopMatrix();
	}
}
void add_object(void)
{
	glGetFloatv(GL_MODELVIEW_MATRIX,damatrix);		//	Read the matrix first
	if (icnt1){
		res_u=res_ulow;
		res_v=res_vlow;
	}
//	base_vert=pscene->nverts;
	for (cnt_v=0;cnt_v<=res_v;cnt_v++)
		for (cnt_u=0;cnt_u<=res_u;cnt_u++)
		{
			cnt_comp=0;
			for (cnt_vrt=0;cnt_vrt<3;cnt_vrt++)
			{
				cal_u=(float)cnt_u/res_u;
				cal_v=(float)cnt_v/res_v;
				switch(cnt_vrt)
				{
					case 1: cal_v +=0.1f/res_v; break;
					case 2: cal_u +=0.1f/res_u; break;
				}

				
//	Calculate coordinates depending on object type
				switch(res_obj)
				{
				case OBJ_PLANE:
					ptmpvert[0]=cal_u*2.f-1.f;
					ptmpvert[1]=1.f;
					ptmpvert[2]=cal_v*2.f-1.f;
					break;
				case OBJ_CUERNO:
/*					__asm {
						fldpi
						fadd	st(0),st(0)
						fmul	dword ptr [cal_u]
						//fstp	dword ptr [facc1]
						fsincos							//sin	cos
						fld1							//1		sin		cos
						fsub	dword ptr [cal_v]		//1-cv  sin		cos
						fld		st(0)					//1-cv	1-cv	sin		cos
						fmul	st

					}*/
					facc1=2.f*PI*cal_u;
					facc2=(2.f+fcos(facc1)*(float)(1-cal_v));
					ptmpvert[0]=facc2*fcos(PI/2.f*cal_v)-2.f;
					ptmpvert[1]=facc2*fsin(PI/2.f*cal_v);
					ptmpvert[2]=fsin(facc1)*(float)(1-cal_v);
					break;
				case OBJ_LOGO:
					for (ktrx=0;ktrx<8;ktrx++) ptmpvert[ktrx]=0;
					for (ktry=2;ktry<6;ktry+=2){
						ptmpvert[0]=0.f;
						for (ktrx=0;ktrx<=33;ktrx++)
						{
							ptmpvert[0]+=2.f*PI*cal_v;
						//	ptmpvert[ktry]+=(float)(fourierx1[ktrx]*fsin(ptmpvert[0]));
						//	ptmpvert[ktry+1]+=(float)(fourierx1[ktrx]*fcos(ptmpvert[0]));

//							ptmpvert[ktry]+=((float)(fourierx1s[ktrx]))*fsin(ptmpvert[0]) / 32767;
//							ptmpvert[ktry+1]+=((float)(fourierx1s[ktrx]))*fcos(ptmpvert[0]) / 32767;
//							ptmpvert[ktry+1]=-ptmpvert[ktry+1];
							ptmpvert[ktry+1]=-ptmpvert[ktry+1]-((float)(fourierx1s[ktrx]))*fcos(ptmpvert[0]) / 32767;
							ptmpvert[ktry]+=((float)(fourierx1s[ktrx]))*fsin(ptmpvert[0]) / 32767;
//							ptmpvert[ktry+1]=-ptmpvert[ktry+1];
							if (ktrx&1) ptmpvert[0]+=2.f*PI*cal_v;
						}
						cal_v+=0.1f/res_v;
					}
					ptmpvert[0]=ptmpvert[2]+(ptmpvert[5]-ptmpvert[3])*fcos(2.f*PI*cal_u)*4.f;
					ptmpvert[1]=ptmpvert[3]-(ptmpvert[4]-ptmpvert[2])*fcos(2.f*PI*cal_u)*4.f;
					ptmpvert[2]=fsin(2.f*PI*cal_u)*0.1f;
					break;
/*				default:
				    break;*/
				}
				ptmpvert[3]=1.f;

//	Transform all coordinates
				for (ktrx=0;ktrx<3;ktrx++)
				{
					ptmppos[cnt_comp]=0.f;
					for (ktry=0;ktry<4;ktry++)
						ptmppos[cnt_comp]+=damatrix[ktrx+ktry*4]*ptmpvert[ktry];
					cnt_comp++;
				}
			}

			for (ktrx=0;ktrx<3;ktrx++) pvert->vpos[ktrx]=ptmppos[ktrx];

//	Generate normal depending on transformed coordinates (JA)

			pvert->vnor[0]= (ptmppos[4]-ptmppos[1])*(ptmppos[8]-ptmppos[2])-
							(ptmppos[5]-ptmppos[2])*(ptmppos[7]-ptmppos[1]);
			pvert->vnor[1]= (ptmppos[5]-ptmppos[2])*(ptmppos[6]-ptmppos[0])-
							(ptmppos[3]-ptmppos[0])*(ptmppos[8]-ptmppos[2]);
			pvert->vnor[2]= (ptmppos[3]-ptmppos[0])*(ptmppos[7]-ptmppos[1])-
							(ptmppos[4]-ptmppos[1])*(ptmppos[6]-ptmppos[0]);

//	Increment number of vertexs 
			if (cnt_u!=res_u && cnt_v!=res_v)
			{
				pquad[0]=pscene->nverts;
				pquad[1]=pscene->nverts+(res_u+1);
				pquad[2]=pscene->nverts+1+(res_u+1);
				pquad[3]=pscene->nverts+1;
				pquad+=4;
				pscene->nquads+=4;
			}
//			base_vert++;
			pvert++;
			pscene->nverts++;	//Hauria de ser un punter???
		}
}
void add_cube(void)
{
	res_obj=OBJ_PLANE;
	res_ulow=res_vlow=1;
	add_object();
	glRotatef(90,1,0,0);
	add_object();
	glRotatef(90,1,0,0);
	add_object();
	glRotatef(90,1,0,0);
	add_object();
	glRotatef(90,0,0,1);
	add_object();
	glRotatef(180,0,0,1);
	add_object();
}

void intro_init_polar(HDC d_hDC)
{
//	int icnt1,icnt2,icnt3,ktrx,ktry;
//	thH = CreateThread (0,0,(LPTHREAD_START_ROUTINE)threadmain,0,0,(LPDWORD)&tID);
//	SetThreadPriority(thH,THREAD_PRIORITY_TIME_CRITICAL);

	font=CreateFont(-1,0,0,0,FW_BOLD,0,0,0,ANSI_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,FF_ROMAN|VARIABLE_PITCH,"arial");
	SelectObject(d_hDC,font);
	wglUseFontOutlines(d_hDC,1,255,   1   ,0.f,0.0f,WGL_FONT_POLYGONS,NULL); //wgl_font_poligons
	glClearColor(1,1,1,1);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(70.0f,1,.2f,30.0f);
	//gluPerspective(60.0f,(GLfloat)SCREEN_WIDTH/(GLfloat)SCREEN_HEIGHT,0.2f,30.0f);
	glMatrixMode(GL_MODELVIEW);
	pscene=sc_normal;
//	res_simple=0;
	for (icnt1=0;icnt1<2;icnt1++){
	  cnt_scn=0;

	  for (demostate=0;demostate<SCENES;demostate++)
	  {
		pvert=pscene->verts;
		pquad=pscene->quads;
		pscene->nquads=0;
		pscene->nverts=0;
		pscene->text_ini=cnt_scn;
		cnt_scn=pscene->text_num=cnt_scn+text_numbers[demostate];
//		pscene->text_num;

		//assumim cuerno per defecte
		res_u=res_v=20;
	    res_ulow=res_vlow=6;
	    res_obj=OBJ_CUERNO;
		
		glLoadIdentity();

		switch(demostate) {
		case 0:	//logo fuzzion
#ifdef GAMMA
			break;
#endif

			glTranslatef(-4.f,0.4f,-0.6f);
			res_u=8;	res_v=200;
			res_ulow=4;	res_vlow=75;
			res_obj=OBJ_LOGO;
			add_object();
			//pla girat
			glLoadIdentity();
			glRotatef(90,1,0,0);
			glTranslatef(0,-1.8f,0);
			res_u=res_v=100;
			res_ulow=res_vlow=1;

		break;

		case 6: //npi
#ifdef GAMMA
			break;
#endif
			holdrand=30;
			for (icnt2=0;icnt2<40;icnt2++)
			{
				glRotatef(myRandFloat()*180,1,0,0);
				glRotatef(myRandFloat()*180,0,1,0);
				glRotatef(myRandFloat()*180,0,0,1);
				glScalef(1,2,1);
				glTranslatef(0.f,-3.f,0);
				add_object();
				glScalef(-1,-1,-1);
				add_object();
				glLoadIdentity();
			}
			glScalef(1.6f,1.6f,1.6f);
			res_u=8;	res_v=200;	
			res_ulow=4;	res_vlow=75;
			res_obj=OBJ_LOGO;
			add_object();
			res_u=res_v=res_ulow=res_vlow=0;	
//			res_u=res_ulow=0;	

		break;

		case 2:
#ifdef GAMMA
			break;
#endif
			//pla

			holdrand=40;
			for (icnt2=0;icnt2<20;icnt2++)
			{
				glRotatef(myRandFloat()*180,1,0,0);
				glRotatef(myRandFloat()*180,0,1,0);
				glRotatef(myRandFloat()*180,0,0,1);
				glScalef(1,2,1);
				add_object();
				glLoadIdentity();
			}
			glTranslatef(0,-5.f,0);
			res_u=res_v=100;
			res_ulow=res_vlow=1;
			break;
		case 1:
		case 4:
				res_u=res_v=10;
			for (icnt3=0;icnt3<3;icnt3++)
			for (icnt2=0;icnt2<4;icnt2++)
			{
//				res_ulow=res_vlow=1;
//				glRotatef(120.f*icnt3,1,1,1);
//				glRotatef(90,1,1,1);
//				glTranslatef(-3,-3.f,-3);
				glRotatef(90.f+120.f*icnt3,1,1,1);
//				glTranslatef(3,3.f,3);
				glRotatef(90.f*icnt2,0,1,0);
				glScalef(0.8f,0.8f,0.8f);
				glTranslatef(3,0,3);
				glScalef(1.f,3,1.f);
//				glScalef(1,3,1);
//				glTranslatef(0,1,0);
				add_cube();
				glLoadIdentity();
			}
//			glLoadIdentity();
			glTranslatef(0,-5.f,0);
			res_u=res_v=100;
			res_ulow=res_vlow=1;
			break;

		case 5:	
#ifdef GAMMA
			break;
#endif
			// Credits
			holdrand=10;
			res_u=res_v=12;
			for (icnt2=0;icnt2<25;icnt2++)
			{
				f_calc=2*(myRandFloat()+1.1f);
				glTranslatef(0,0,-3.f);
				glTranslatef(myRandFloat() * 10,myRandFloat() * 8,myRandFloat());
				glScalef(f_calc,f_calc,0.4f);
				add_cube();
				glLoadIdentity();
			}
			glRotatef(90,1,0,0);
			glTranslatef(0,-5.f,0);
			res_u=res_v=100;
			res_ulow=res_vlow=1;
			break;
		case 3:
#ifdef GAMMA
			break;
#endif
			holdrand=10;	//23 mola
//			res_ulow=res_vlow=1;
			res_u=res_v=4;
			for (icnt2=0;icnt2<100;icnt2++)
			{
				f_calc=0.2f*(myRandFloat()+1.1f);
				glTranslatef(myRandFloat() * 4,myRandFloat(),myRandFloat());
				glScalef(f_calc,f_calc,f_calc);
				add_cube();
				glLoadIdentity();
			}
			glRotatef(90,1,0,0);
			glTranslatef(0,-1.8f,0);
			res_u=res_v=100;
			res_ulow=res_vlow=1;
			break;
		}
		res_obj=OBJ_PLANE;
		glScalef(10,0.1f,10);
		add_object();
		pscene++;
	  }
		//	prepare for simple scene
//	  res_simple=1;
	  pscene=sc_simple;
	}

//	demostate = 0;
//	still simple scene selected
//	Generate the lightmaps

//no hauria de ser demostate :P!
//	iacc1=0;
//	for (cnt_scn=0;cnt_scn<SCENES;cnt_scn++)  iacc1+=sc_normal[cnt_scn].nverts/(16*11);//;((SCREEN_HEIGHT/pantsize)*(SCREEN_WIDTH/pantsize));
	cnt_vrt=0;
//	printf("%d\n",iacc1);
#ifndef NO_SHADE
	for(icnt1=0;icnt1<SCENES;icnt1++)
	{
		pscene=sc_simple+icnt1;
		glInterleavedArrays(GL_N3F_V3F,sizeof(str_vertex),(float*)(pscene->verts)+4);
/*		for (icnt2=0;icnt2<pscene->nverts;icnt2++){
			tmp_verts[icnt2][0]=pscene->verts[icnt2].vpos[0];
			tmp_verts[icnt2][1]=pscene->verts[icnt2].vpos[1];
			tmp_verts[icnt2][2]=pscene->verts[icnt2].vpos[2];
		}
		glInterleavedArrays(GL_V3F,sizeof(float)*3,(float*)(tmp_verts));*/
		pvert=sc_normal[icnt1].verts;
//		for (cal_u=0.0f;cal_u<1.2f;cal_u+=0.1f) printf ("%f %f\n",cal_u,fpow(cal_u,2.f));
		for (icnt2=0;icnt2<(sc_normal+icnt1)->nverts;icnt2++)
		{
			ktrx=((cnt_vrt)&0x3C0);//((cnt_vrt/iacc1)%(SCREEN_WIDTH/pantsize))*pantsize;
			ktry=(((cnt_vrt/16))&0x3C0);//((cnt_vrt/iacc1/(SCREEN_WIDTH/pantsize))%(SCREEN_HEIGHT/pantsize))*pantsize;
#ifdef ALTERNATE_LOADING
			if ((cnt_vrt)==0)
			{
				ptexts=scene_texts+6;

				glViewport(0,0,1024,768);
				glLoadIdentity();

				glTranslatef(ptexts->vpos[0],ptexts->vpos[1],-4);
				glCallLists(ptexts->num,GL_UNSIGNED_BYTE,ptexts->dastr);

				if ((icnt2&0x1C0)==0)SwapBuffers(d_hDC);
				glClear(GL_COLOR_BUFFER_BIT);
			}
			cnt_vrt=(cnt_vrt+pantsize)%0x3000;
#else
			if ((icnt2%950)==0)
			{
				ptexts=scene_texts+6;

				glViewport(0,0,1024,768);
				glLoadIdentity();

				glTranslatef(ptexts->vpos[0],ptexts->vpos[1],-4);
				glCallLists(ptexts->num,GL_UNSIGNED_BYTE,ptexts->dastr);

				SwapBuffers(d_hDC);
				cnt_vrt+=pantsize;
			}

			glClear(GL_COLOR_BUFFER_BIT);
#endif
			glViewport(ktrx,ktry,pantsize,pantsize);
			glLoadIdentity();
			gluLookAt(	pvert->vpos[0],pvert->vpos[1],pvert->vpos[2],
						pvert->vpos[0]+pvert->vnor[0],
						pvert->vpos[1]+pvert->vnor[1],
						pvert->vpos[2]+pvert->vnor[2],
						0.01f,1.f,0);
			
//			glInterleavedArrays(GL_N3F_V3F,sizeof(str_vertex),(float*)(pscene->verts)+4);
			draw_scena();
			glReadPixels(ktrx,ktry,pantsize,pantsize,GL_BLUE,GL_UNSIGNED_BYTE,ipantalla);
//			glRead
			//icnt3=0;
//			for (icnt2=0;icnt2<pantsize*pantsize;icnt2++) pvert->hdr_color+=ipantalla[icnt2];
//			for (icnt2=0;icnt2<pantsize*pantsize;icnt2++) icnt3+=ipantalla[icnt2];

			__asm {
				xor		ebx,ebx
				mov		ecx,4096
				lea		esi,ipantalla
		_adder:
				lodsb
				movzx	eax,al
				add		ebx,eax
				loop	_adder

//				push	ebx
//				fild	dword ptr [esp]
//				fidiv	dword ptr [cnt1Mb]

//				lea		ebx, pvert
//				//vcol is the first 4 floats
//				fst		dword ptr [ebx + 0]
//				fst		dword ptr [ebx + 4]
//				fstp	dword ptr [ebx + 8]
//			}

				mov		[icnt3], ebx
			}
			facc1 = (float)icnt3/(pantsize*pantsize*256);//1048576.f;
			pvert->vcol[0] = facc1;
			pvert->vcol[1] = facc1;
			pvert->vcol[2] = facc1;

			/*pvert->vcol[0] = (float)icnt3/(pantsize*pantsize*256);
			pvert->vcol[1] = (float)icnt3/(pantsize*pantsize*256);
			pvert->vcol[2] = (float)icnt3/(pantsize*pantsize*256);*/

			//for (iacc1=0;iacc1<3;iacc1++) pvert->vcol[iacc1]=(float)icnt3/(pantsize*pantsize*256);//fpow((float)icnt3/(pantsize*pantsize*256),1.6f);

	//		printf("%d\n",pvert->hdr_color);

			pvert++;
		}
	}
#endif
	//glShadeModel(GL_SMOOTH);

//	order = 1;
//	row= 0;

}


int intro_init( HDC d_hDC )
{
    if( !EXT_Init() )
        return( 0 );
	intro_init_polar(d_hDC);
    return 1;
}


void intro_frame( long time )
{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	


	glViewport(0,0,SCREEN_XRES,SCREEN_YRES);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60.0f,(GLfloat)SCREEN_XRES/(GLfloat)SCREEN_YRES,0.2f,30.0f);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

//	Set Camera	

	demostate = order % 7;
	if(order < 46) demostate = 5;
	if(order < 38) demostate = 6;
	if(order < 34) demostate = 1;
	if(order < 29) demostate = 6;
	if(order < 22) demostate = 3;
	if(order < 14) demostate = 2;
	if(order < 10) demostate = 0;

//	demostate = 6;
#ifdef GAMMA
	demostate=1;
#endif

	pscene=sc_normal+demostate;
#ifdef LO_RES
	pscene=sc_simple+demostate;
#endif

	//printf("%d - %d\n",order, demostate);
/*
	ktrx=float2int((float)ts*0.000564f-0.5f);
	facc1=ts*0.000564f-(float)ktrx;
	facc1=0.5f*(1.f-fcos(facc1*PI));
	f_calc=PI/180.f*interpol.rot;
	for (ktry=0;ktry<5 && text_numbers[demostate];ktry++)
	{
		ktrx%=text_numbers[demostate];//pscene->text_num-pscene->text_ini;
		interpol.vpos[ktry]=scene_texts[ktrx+pscene->text_ini].vpos[ktry]*(1-facc1)+
		scene_texts[(ktrx+1)%(text_numbers[demostate])+pscene->text_ini].vpos[ktry]*facc1;
//			scene_texts[(ktrx+1)%(pscene->text_num-pscene->text_ini)+pscene->text_ini].vpos[ktry]*facc1;
	}
*/
	ptmppos[1]=ptmppos[0]=time*0.0008f;

	switch(demostate) {
//	case 0:
//		gluLookAt(-2+ 5*fsin(ts*0.0005f),2.f*fcos(ts*0.0001f),8.f,0,2,0,0,1,0);
//		break;
	case 6:
//		gluLookAt(12.f*(fsin(ts*0.001f+order)),5.f*((1-fcos(ts*0.001f+order))),-12.f*fcos(ts*0.001f+order),0,0,0,0,1,0);
		ptmppos[1]+=order; //JARL: RECORDA DE CAMBIAR
//		break;
	case 1:
#ifdef GAMMA
		gluLookAt(0.f,0.f,-12.f,0,0,0,0,1,0);
		break;
#endif
	case 4:
//		gluLookAt(12.f*fsin(ts*0.001f+order),5.f*(1-fcos(ts*0.0001f+order)),-12.f*fcos(ts*0.001f),0,0,0,0,1,0);
		ptmppos[0]+=order; //JARL: RECORDA DE CAMBIAR
//		break;
//	case 4:
	case 2:
//		gluLookAt(12.f*fsin(ts*0.001f),5.f*(1-fcos(ts*0.0001f)),-12.f*fcos(ts*0.001f),0,0,0,0,1,0);
		gluLookAt(12.f*fsin(ptmppos[0]),5.f*(1-fcos(ptmppos[0])),-12.f*fcos(ptmppos[1]),0,0,0,0,1,0);
		break;
//	case 0:
	default:
//		printf("%d %d %d\n",demostate,pscene->text_ini,pscene->text_num);
		gluLookAt(-2+ 5*fsin(time*0.0005f),2.f*fcos(time*0.0001f),8.f,0,0,0,0,1,0);
/*
		gluLookAt(interpol.vpos[0]+interpol.size*(2*fcos(f_calc)+4*fsin(f_calc)),
				interpol.vpos[1],
				interpol.vpos[2]+interpol.size*(4*fcos(f_calc)-2*fsin(f_calc)),
				interpol.vpos[0]+interpol.size*2*fcos(f_calc),
				interpol.vpos[1],
				interpol.vpos[2]+interpol.size*(-2*fsin(f_calc)),
				0.01f,1.f,0);*/
		break;
	}
	//logo fuzzion:
	//
//	Draw Scene

#ifdef NO_SHADE
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_NORMALIZE);
#endif
	glEnable(GL_DEPTH_TEST);
//	glCullFace(GL_FRONT);


//	pvert=pscene->verts;
/*
	for (icnt1=0;icnt1<pscene->nverts;icnt1++)
	{
		for (icnt2=0;icnt2<3;icnt2++)
			pvert->vcol[icnt2]=(float)pvert->hdr_color/(pantsize*pantsize*255);//*(1.f+fsin(ts*0.00045f))-fsin(ts*0.00072f);
		pvert++;

	}*/

	
	glInterleavedArrays(GL_C4F_N3F_V3F,sizeof(str_vertex),(float*)(pscene->verts));
	draw_scena();

}