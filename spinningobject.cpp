// book_spin.cpp
#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>
#include <thread>
#include <algorithm>

//Testing my code
struct Vec3 { float x,y,z; };
static inline Vec3 add(const Vec3&a,const Vec3&b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
static inline Vec3 mul(const Vec3&a,float k){return {a.x*k,a.y*k,a.z*k};}
static inline float dot(const Vec3&a,const Vec3&b){return a.x*b.x+a.y*b.y+a.z*b.z;}

int main() {
    using namespace std::chrono_literals;

    // Screen
    const int W=90, H=30;
    const char shades[] = ".,-~:;=!*#$@";
    const int nShades = sizeof(shades)-1;

    // Book dimensions (world units)
    const float BOOK_W = 3.2f;   // cover width (x)
    const float BOOK_H = 4.6f;   // height (y)
    const float BOOK_T = 0.65f;  // thickness (z)
    const float SPINE_R = 0.28f; // rounded spine radius (half-cylinder)

    // Steps (surface sampling density)
    const float STEP_U = 0.10f;   // along width / height
    const float STEP_V = 0.10f;
    const float STEP_THETA = 0.12f; // for spine curve

    auto project = [&](const Vec3& p, int& sx, int& sy, float& iz){
        // Simple perspective
        float z = p.z + 7.0f;        // push forward
        float inv = 1.0f / z;
        sx = (int)(W/2 + 32.0f*inv*p.x);
        sy = (int)(H/2 - 18.0f*inv*p.y);
        iz = inv;
    };

    auto rotate = [&](const Vec3& p, float A, float B){
        // Rotate around X (A), then Y (B)
        float cA=std::cos(A), sA=std::sin(A);
        float cB=std::cos(B), sB=std::sin(B);
        // X rotation
        float y1 =  cA*p.y - sA*p.z;
        float z1 =  sA*p.y + cA*p.z;
        // Y rotation
        float x2 =  cB*p.x + sB*z1;
        float z2 = -sB*p.x + cB*z1;
        return Vec3{x2,y1,z2};
    };

    auto rotateN = rotate; // same for normals (no translation)

    // Light direction (world space), normalized-ish
    const Vec3 Lw = { -0.4f, 0.6f, 1.0f };

    float A=0.f, B=0.f;
    std::cout << "\x1b[2J\x1b[?25l" << std::flush; // clear & hide cursor

    while(true){
        std::vector<char> fb(W*H,' ');
        std::vector<float> zb(W*H, 0.0f);

        auto plot = [&](const Vec3& Pw, const Vec3& Nw){
            // Rotate into camera space
            Vec3 Pr = rotate(Pw, A, B);
            Vec3 Nr = rotateN(Nw, A, B);

            // Lighting
            float L = dot(Nr, Lw);
            // Bias to [0,1]
            float lit = std::max(0.f, std::min(1.f, 0.5f*L + 0.5f));
            int shade = std::max(0, std::min(nShades-1, (int)(lit*(nShades-1))));

            // Project
            int sx, sy; float iz;
            project(Pr, sx, sy, iz);
            if(sx<0||sx>=W||sy<0||sy>=H) return;

            int idx = sx + sy*W;
            if(iz > zb[idx]){
                zb[idx]=iz;
                fb[idx]=shades[shade];
            }
        };

        // ====== Build surfaces ======
        float xL = -BOOK_W/2, xR = BOOK_W/2;
        float yB = -BOOK_H/2, yT = BOOK_H/2;
        float zF = -BOOK_T/2, zB_ = BOOK_T/2;

        // 1) Front cover (z = zF), normal (0,0,-1)
        for(float y=yB; y<=yT; y+=STEP_V)
            for(float x=xL; x<=xR; x+=STEP_U)
                plot({x,y,zF}, {0,0,-1});

        // 2) Back cover (z = zB_), normal (0,0,1)
        for(float y=yB; y<=yT; y+=STEP_V)
            for(float x=xL; x<=xR; x+=STEP_U)
                plot({x,y,zB_}, {0,0,1});

        // 3) Top & bottom edges (pages)
        // Top (y=yT), normal (0,1,0)
        for(float z=zF; z<=zB_; z+=STEP_U)
            for(float x=xL; x<=xR; x+=STEP_U){
                // Add faint stripes to mimic pages
                float stripe = 0.15f*std::sin(20.0f*x);
                plot({x,yT,z+stripe}, {0,1,0});
            }
        // Bottom (y=yB), normal (0,-1,0)
        for(float z=zF; z<=zB_; z+=STEP_U)
            for(float x=xL; x<=xR; x+=STEP_U){
                float stripe = 0.15f*std::sin(20.0f*x);
                plot({x,yB,z+stripe}, {0,-1,0});
            }

        // 4) Page edge (right face, x=xR), normal (1,0,0)
        for(float y=yB; y<=yT; y+=STEP_V)
            for(float z=zF; z<=zB_; z+=STEP_U){
                float stripe = 0.12f*std::sin(22.0f*y); // vertical page grooves
                plot({xR,y,z+stripe}, {1,0,0});
            }

        // 5) Hinge face near left (flat bit before spine), x ~ xL
        // a small flat vertical strip to transition to the spine
        float hinge = xL + 0.10f;
        for(float y=yB; y<=yT; y+=STEP_V)
            for(float z=zF; z<=zB_; z+=STEP_U)
                plot({hinge,y,z}, {-1,0,0});

        // 6) Rounded spine (half-cylinder centered at the left edge)
        // Axis: vertical (y). Curve across x-z plane from pi/2..3pi/2
        // Center the cylinder at (xL+SPINE_R, 0, 0)
        Vec3 C = {xL+SPINE_R, 0.0f, 0.0f};
        for(float y=yB; y<=yT; y+=STEP_V){
            for(float t=1.0f; t<=3.14159f-1.0f; t+=STEP_THETA){
                float ct = std::cos(t), st = std::sin(t);
                Vec3 p = { C.x + SPINE_R*ct, y, SPINE_R*st };
                Vec3 n = { ct, 0.0f, st }; // outward normal of the cylinder
                plot(p, n);
            }
        }

        // Draw frame
        std::cout << "\x1b[H";
        for(int r=0;r<H;++r){
            std::cout.write(&fb[r*W], W);
            std::cout.put('\n');
        }
        std::cout.flush();

        // Animate
        A += 0.035f;  // tip toward/away
        B += 0.020f;  // spin left/right
        std::this_thread::sleep_for(28ms);
    }

    // (Unreachable) show cursor again
    // std::cout << "\x1b[?25h\n";
}
