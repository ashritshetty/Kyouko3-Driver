float translate(float triangle[6], float xscale, float yscale)
{
  float rtriangle[6];
  rtriangle[0] = triangle[0] + xscale;
  rtriangle[1] = triangle[1] + yscale;
  rtriangle[2] = triangle[2] + xscale;
  rtriangle[3] = triangle[3] + yscale;
  rtriangle[4] = triangle[4] + xscale;
  rtriangle[5] = triangle[5] + yscale;

  return rtriangle;
}

float rotate(float triangle[6], float sino)
{
  float coso = sqrt(1-(sino*sino));
  float rtriangle[6];
  rtriangle[0] = triangle[0]*coso - triangle[1]*sino;
  rtriangle[1] = triangle[0]*sino + triangle[1]*coso;
  rtriangle[2] = triangle[2]*coso - triangle[3]*sino;
  rtriangle[3] = triangle[2]*sino + triangle[3]*coso;
  rtriangle[4] = triangle[4]*coso - triangle[5]*sino;
  rtriangle[5] = triangle[4]*sino + triangle[5]*coso;

  return rtriangle;
}

float check(float triangle[6])
{
  int i;
  float rtriangle[6];
  for(i = 0; i < 6; i++)
  {
    if(triangle[i] < -1.0)
      rtriangle[i] = -1.0;
    else if(triangle[i] > 1.0)
      rtriangle[i] = 1.0;
    else
      rtriangle[i] = triangle[i];
  }

  return rtriangle;
}

void draw(unsigned int* temp_addr, float triangle[6])
{
  float x[3] = {triangle[0], triangle[2], triangle[4]};
  float y[3] = {triangle[1], triangle[3], triangle[5]};
  float z[3] = {0.0, 0.0, 0.0};
  float w[3] = {1.0, 1.0, 1.0};

  float r[3] = {1.0, 0.0, 0.0};
  float b[3] = {0.0, 1.0, 0.0};
  float g[3] = {0.0, 0.0, 1.0};
  float a[3] = {0.0, 0.0, 0.0};

  int i;
  //Format is BGRXYZ
  for(i = 0; i < 3; i++)
  {
    //Writing blue color
    *temp_addr = *(unsigned int*)&b[i];
    temp_addr++;

    //Writing green color
    *temp_addr = *(unsigned int*)&g[i];
    temp_addr++;

    //Writing red color
    *temp_addr = *(unsigned int*)&r[i];
    temp_addr++;

    //Writing X-coord
    x[i] *= factor;
    *temp_addr = *(unsigned int*)&x[i];
    temp_addr++;

    //Writing Y-coord
    y[i] *= factor;
    *temp_addr = *(unsigned int*)&y[i];
    temp_addr++;

    //Writing Z-coord
    z[i] *= factor;
    *temp_addr = *(unsigned int*)&z[i];
    temp_addr++;
  }
}

float i, j;
float dtriangle[] = {x1, y1, x2, y2, x3, y3};
for(i = -1.0, i < 1.0, i = i+scale)
{
  for(j = -1.0, j < 1.0, j = j+scale)
  {
    dtriangle = translate(dtriangle, xscale, yscale);
    dtriangle = rotate(dtriangle, j);
    dtriangle = check(dtriangle);

    *temp_addr = *(unsigned int*)&k_dma_header;
    temp_addr++;

    draw(temp_addr, dtriangle);
    dma_addr = 76;
    ioctl(fd, START_DMA, &dma_addr);
    temp_addr = (unsigned int*)dma_addr;

    entry.cmd = FIFO_FLUSH_REG;
    entry.value = 0;
    ioctl(fd, FIFO_QUEUE, &entry);
    xscale = xscale + scale;
  }
  yscale = yscale + scale;
}
