pip3 install cantools
python3 -m cantools generate_c_source chery_canfd.dbc

in bashrc u can add
alias cabana='/home/omoda/tools/cabana/cabana --socketcan can0 --socketcan-fd --socketcan-brs --dbc /home/omoda/tools/cabana/opendbc/chery_canfd_dbc/chery_canfd.dbc'
