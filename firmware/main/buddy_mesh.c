#include "buddy_mesh.h"
#include "buddy_expression.h"
#include <math.h>
typedef struct {float x,y,z;} vec_t;
typedef struct {buddy_face_t *out;size_t count;float yaw,pitch,bob;unsigned layer;} scene_t;
static vec_t rotate(vec_t p,float yaw,float pitch) {
    float c=cosf(yaw),s=sinf(yaw),q=cosf(pitch),r=sinf(pitch);
    float x=p.x*c+p.z*s,z=-p.x*s+p.z*c;
    return (vec_t){x,p.y*q-z*r,p.y*r+z*q};
}
static uint32_t shade(uint32_t rgb,float f) {
    unsigned r=((rgb>>16)&255)*f,g=((rgb>>8)&255)*f,b=(rgb&255)*f;
    return (r<<16)|(g<<8)|b;
}
static void box(scene_t *s,float x,float y,float z,float w,float h,float d,uint32_t color,float head) {
    vec_t v[8];
    for(int i=0;i<8;++i) {
        vec_t p={x+((i&1)?w:-w)/2,y+((i&2)?h:-h)/2,z+((i&4)?d:-d)/2};
        if(head) {p.y+=23;p=rotate(p,head,0);p.y-=23;}
        v[i]=rotate(p,s->yaw,s->pitch);v[i].y+=s->bob;
    }
    static const unsigned faces[6][4]={{0,2,3,1},{4,5,7,6},{0,4,6,2},{1,3,7,5},{0,1,5,4},{2,6,7,3}};
    for(int f=0;f<6;++f) {
        vec_t a=v[faces[f][0]],b=v[faces[f][1]],c=v[faces[f][2]];
        vec_t u={b.x-a.x,b.y-a.y,b.z-a.z},t={c.x-a.x,c.y-a.y,c.z-a.z};
        vec_t n={u.y*t.z-u.z*t.y,u.z*t.x-u.x*t.z,u.x*t.y-u.y*t.x};
        if(n.x*a.x+n.y*a.y+n.z*(a.z+260)>=0)continue;
        if(s->count>=BUDDY_MAX_FACES)return;
        buddy_face_t *o=&s->out[s->count++];o->depth=0;o->layer=s->layer;
        float norm=sqrtf(n.x*n.x+n.y*n.y+n.z*n.z);
        float light=(-n.x*.35f-n.y*.6f-n.z*.72f)/norm;
        o->rgb=shade(color,.55f+.4f*fmaxf(0,light));
        for(int j=0;j<4;++j) {
            vec_t p=v[faces[f][j]];float scale=245/(260+p.z);
            o->p[j]=(buddy_point_t){100+p.x*scale,64+p.y*scale};o->depth+=p.z/4;
        }
    }
}
// Face graphics are emissive planar geometry, attached to the rotating head.
// Separate decal layers keep coplanar eyes from being hidden by the face panel.
static void panel(scene_t *s,float x,float y,float w,float h,float tilt,float head,uint32_t rgb) {
    if(s->count>=BUDDY_MAX_FACES)return;
    buddy_face_t *o=&s->out[s->count++];o->depth=0;o->layer=s->layer;o->rgb=rgb;
    const float corners[4][2]={{-1,-1},{1,-1},{1,1},{-1,1}};
    for(int i=0;i<4;++i) {
        float u=corners[i][0]*w/2,v=corners[i][1]*h/2;
        vec_t p={x+u*cosf(tilt)-v*sinf(tilt),y+u*sinf(tilt)+v*cosf(tilt),-20};
        p.y+=23;p=rotate(p,head,0);p.y-=23;
        p=rotate(p,s->yaw,s->pitch);p.y+=s->bob;
        float scale=245/(260+p.z);
        o->p[i]=(buddy_point_t){100+p.x*scale,64+p.y*scale};o->depth+=p.z/4;
    }
}
static void eye(scene_t *s,float x,float y,float height,float tilt,float head,buddy_eye_shape_t shape,bool blink,uint32_t color) {
    // Two passes: a restrained dark halo followed by a bright cyan silhouette.
    for(int pass=0;pass<2;++pass) {
        float pad=pass?0:1.3f;uint32_t rgb=pass?color:shade(color,.22f);
        s->layer=pass?3:2;
        if(shape==E_CROSS) {
            panel(s,x,y,19+pad,4+pad,.72f,head,rgb);panel(s,x,y,19+pad,4+pad,-.72f,head,rgb);
        }else if(shape==E_HAPPY && !blink) {
            panel(s,x-4,y,11+pad,4+pad,-.58f,head,rgb);panel(s,x+4,y,11+pad,4+pad,.58f,head,rgb);
        }else if(height<5)panel(s,x,y,16+pad,height+pad,tilt,head,rgb);
        else {
            panel(s,x,y,16+pad,height-4+pad,tilt,head,rgb);
            // Insets approximate rounded corners without textures or frame buffers.
            float dy=(height-2)/2;
            panel(s,x+sinf(tilt)*dy,y-cosf(tilt)*dy,12+pad,2+pad,tilt,head,rgb);
            panel(s,x-sinf(tilt)*dy,y+cosf(tilt)*dy,12+pad,2+pad,tilt,head,rgb);
        }
    }
}
size_t buddy_mesh(buddy_state_t state,unsigned frame,buddy_face_t out[BUDDY_MAX_FACES]) {
    float t=frame*.10f, wave=sinf(t*3),slow=sinf(t*1.3f);
    bool sleep=state==B_SLEEP,typing=state==B_CODE||state==B_BUILD||state==B_TEST;
    bool happy=state==B_DONE||state==B_WAVE;
    uint32_t accent=state==B_ERROR||state==B_RECONNECT?0xEF7A78:
        state==B_WAIT||state==B_APPROVE||state==B_CONNECT?0xF4C86B:0x8CEBC3;
    scene_t s={out,0,.30f+slow*.16f,-.16f,sleep?7:wave*1.5f,0};
    float head=state==B_SEARCH?slow*.36f:state==B_THINK||state==B_PLAN?-.20f:slow*.06f;
    if(state==B_ERROR)s.yaw+=sinf(t*16)*.08f;
    if(happy)s.bob-=fabsf(wave)*4;
    box(&s,-11,43,0,14,9,22,0xA3B6BD,0);box(&s,11,43,0,14,9,22,0xA3B6BD,0);
    box(&s,0,15,0,38,39,25,0xDDE4DE,0);
    box(&s,-27,typing?18+wave*4:happy?1+wave*8:17,0,12,27,14,0xDDE4DE,0);
    box(&s,27,typing?18-wave*4:happy?-8-wave*9:state==B_THINK?-2:17,0,12,27,14,0xDDE4DE,0);
    box(&s,0,-23,0,74,49,34,0x374957,head);
    s.layer=1;box(&s,0,-23,-18,67,42,2,0x050C13,head);
    buddy_expression_t expression=buddy_expression(state,frame);
    eye(&s,-16+expression.gaze_x,-23+expression.gaze_y,expression.left_height,
        expression.left_tilt,head,expression.shape,expression.blink,expression.color);
    eye(&s,16+expression.gaze_x,-23+expression.gaze_y,expression.right_height,
        expression.right_tilt,head,expression.shape,expression.blink,expression.color);
    s.layer=1;box(&s,0,11,-14,23,15,3,0x16303E,0);s.layer=2;box(&s,0,11,-16,15,6,2,accent,0);
    s.layer=0;box(&s,0,-47,0,3,10,3,0x8798A2,head);box(&s,0,-53,0,7,5,6,accent,head);
    for(size_t i=1;i<s.count;++i) {
        buddy_face_t face=out[i];size_t j=i;
        while(j && (out[j-1].layer>face.layer || (out[j-1].layer==face.layer && out[j-1].depth<face.depth))){out[j]=out[j-1];--j;}
        out[j]=face;
    }
    return s.count;
}
