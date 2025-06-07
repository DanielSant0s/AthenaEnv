; 2025 - Daniel Santos
; AthenaEnv VU0

.syntax new
.name VU0MatrixMul
.vu
.init_vf_all
.init_vi_all

--enter
    in_vf vec1 (VF01)
    in_vf vec2 (VF02)
    in_vf vec3 (VF03)
    in_vf vec4 (VF04)
    in_vf tmpx (VF05)
    in_vf tmpy (VF06)
    in_vf tmpz (VF07)
    in_vf tmpw (VF08)
--endenter
    mul            acc,           tmpx, vec1[x]
    madd           acc,           tmpy, vec1[y]
    madd           acc,           tmpz, vec1[z]
    madd           out1,          tmpw, vec1[w]

    mul            acc,           tmpx, vec2[x]
    madd           acc,           tmpy, vec2[y]
    madd           acc,           tmpz, vec2[z]
    madd           out2,          tmpw, vec2[w]

    mul            acc,           tmpx, vec3[x]
    madd           acc,           tmpy, vec3[y]
    madd           acc,           tmpz, vec3[z]
    madd           out3,          tmpw, vec3[w]

    mul            acc,           tmpx, vec4[x]
    madd           acc,           tmpy, vec4[y]
    madd           acc,           tmpz, vec4[z]
    madd           out4,          tmpw, vec4[w]
--barrier

--exit
    out_vf out1 (VF10)
    out_vf out2 (VF11)
    out_vf out3 (VF12)
    out_vf out4 (VF13)
--endexit