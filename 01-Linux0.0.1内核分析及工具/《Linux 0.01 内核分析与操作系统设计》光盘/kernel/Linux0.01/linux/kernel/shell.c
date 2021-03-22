



char *keybuf;


void os_shell()
{
      unsigned char a;

      a=keybuf;
      if(a==0x2){
 	printk("1");
      }
      else{
      	printk(" %x ",a);
      }
}

